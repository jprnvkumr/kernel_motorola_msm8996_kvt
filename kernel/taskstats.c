/*
 * taskstats.c - Export per-task statistics to userland
 *
 * Copyright (C) Shailabh Nagar, IBM Corp. 2006
 *           (C) Balbir Singh,   IBM Corp. 2006
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/taskstats_kern.h>
#include <linux/delayacct.h>
#include <net/genetlink.h>
#include <asm/atomic.h>

static DEFINE_PER_CPU(__u32, taskstats_seqnum) = { 0 };
static int family_registered;
kmem_cache_t *taskstats_cache;

static struct genl_family family = {
	.id		= GENL_ID_GENERATE,
	.name		= TASKSTATS_GENL_NAME,
	.version	= TASKSTATS_GENL_VERSION,
	.maxattr	= TASKSTATS_CMD_ATTR_MAX,
};

static struct nla_policy taskstats_cmd_get_policy[TASKSTATS_CMD_ATTR_MAX+1]
__read_mostly = {
	[TASKSTATS_CMD_ATTR_PID]  = { .type = NLA_U32 },
	[TASKSTATS_CMD_ATTR_TGID] = { .type = NLA_U32 },
};


static int prepare_reply(struct genl_info *info, u8 cmd, struct sk_buff **skbp,
			void **replyp, size_t size)
{
	struct sk_buff *skb;
	void *reply;

	/*
	 * If new attributes are added, please revisit this allocation
	 */
	skb = nlmsg_new(size);
	if (!skb)
		return -ENOMEM;

	if (!info) {
		int seq = get_cpu_var(taskstats_seqnum)++;
		put_cpu_var(taskstats_seqnum);

		reply = genlmsg_put(skb, 0, seq,
				family.id, 0, 0,
				cmd, family.version);
	} else
		reply = genlmsg_put(skb, info->snd_pid, info->snd_seq,
				family.id, 0, 0,
				cmd, family.version);
	if (reply == NULL) {
		nlmsg_free(skb);
		return -EINVAL;
	}

	*skbp = skb;
	*replyp = reply;
	return 0;
}

static int send_reply(struct sk_buff *skb, pid_t pid, int event)
{
	struct genlmsghdr *genlhdr = nlmsg_data((struct nlmsghdr *)skb->data);
	void *reply;
	int rc;

	reply = genlmsg_data(genlhdr);

	rc = genlmsg_end(skb, reply);
	if (rc < 0) {
		nlmsg_free(skb);
		return rc;
	}

	if (event == TASKSTATS_MSG_MULTICAST)
		return genlmsg_multicast(skb, pid, TASKSTATS_LISTEN_GROUP);
	return genlmsg_unicast(skb, pid);
}

static int fill_pid(pid_t pid, struct task_struct *pidtsk,
		struct taskstats *stats)
{
	int rc;
	struct task_struct *tsk = pidtsk;

	if (!pidtsk) {
		read_lock(&tasklist_lock);
		tsk = find_task_by_pid(pid);
		if (!tsk) {
			read_unlock(&tasklist_lock);
			return -ESRCH;
		}
		get_task_struct(tsk);
		read_unlock(&tasklist_lock);
	} else
		get_task_struct(tsk);

	/*
	 * Each accounting subsystem adds calls to its functions to
	 * fill in relevant parts of struct taskstsats as follows
	 *
	 *	rc = per-task-foo(stats, tsk);
	 *	if (rc)
	 *		goto err;
	 */

	rc = delayacct_add_tsk(stats, tsk);
	stats->version = TASKSTATS_VERSION;

	/* Define err: label here if needed */
	put_task_struct(tsk);
	return rc;

}

static int fill_tgid(pid_t tgid, struct task_struct *tgidtsk,
		struct taskstats *stats)
{
	struct task_struct *tsk, *first;
	unsigned long flags;

	/*
	 * Add additional stats from live tasks except zombie thread group
	 * leaders who are already counted with the dead tasks
	 */
	first = tgidtsk;
	if (!first) {
		read_lock(&tasklist_lock);
		first = find_task_by_pid(tgid);
		if (!first) {
			read_unlock(&tasklist_lock);
			return -ESRCH;
		}
		get_task_struct(first);
		read_unlock(&tasklist_lock);
	} else
		get_task_struct(first);

	/* Start with stats from dead tasks */
	spin_lock_irqsave(&first->signal->stats_lock, flags);
	if (first->signal->stats)
		memcpy(stats, first->signal->stats, sizeof(*stats));
	spin_unlock_irqrestore(&first->signal->stats_lock, flags);

	tsk = first;
	read_lock(&tasklist_lock);
	do {
		if (tsk->exit_state == EXIT_ZOMBIE && thread_group_leader(tsk))
			continue;
		/*
		 * Accounting subsystem can call its functions here to
		 * fill in relevant parts of struct taskstsats as follows
		 *
		 *	per-task-foo(stats, tsk);
		 */
		delayacct_add_tsk(stats, tsk);

	} while_each_thread(first, tsk);
	read_unlock(&tasklist_lock);
	stats->version = TASKSTATS_VERSION;

	/*
	 * Accounting subsytems can also add calls here to modify
	 * fields of taskstats.
	 */

	return 0;
}


static void fill_tgid_exit(struct task_struct *tsk)
{
	unsigned long flags;

	spin_lock_irqsave(&tsk->signal->stats_lock, flags);
	if (!tsk->signal->stats)
		goto ret;

	/*
	 * Each accounting subsystem calls its functions here to
	 * accumalate its per-task stats for tsk, into the per-tgid structure
	 *
	 *	per-task-foo(tsk->signal->stats, tsk);
	 */
	delayacct_add_tsk(tsk->signal->stats, tsk);
ret:
	spin_unlock_irqrestore(&tsk->signal->stats_lock, flags);
	return;
}


static int taskstats_send_stats(struct sk_buff *skb, struct genl_info *info)
{
	int rc = 0;
	struct sk_buff *rep_skb;
	struct taskstats stats;
	void *reply;
	size_t size;
	struct nlattr *na;

	/*
	 * Size includes space for nested attributes
	 */
	size = nla_total_size(sizeof(u32)) +
		nla_total_size(sizeof(struct taskstats)) + nla_total_size(0);

	memset(&stats, 0, sizeof(stats));
	rc = prepare_reply(info, TASKSTATS_CMD_NEW, &rep_skb, &reply, size);
	if (rc < 0)
		return rc;

	if (info->attrs[TASKSTATS_CMD_ATTR_PID]) {
		u32 pid = nla_get_u32(info->attrs[TASKSTATS_CMD_ATTR_PID]);
		rc = fill_pid(pid, NULL, &stats);
		if (rc < 0)
			goto err;

		na = nla_nest_start(rep_skb, TASKSTATS_TYPE_AGGR_PID);
		NLA_PUT_U32(rep_skb, TASKSTATS_TYPE_PID, pid);
		NLA_PUT_TYPE(rep_skb, struct taskstats, TASKSTATS_TYPE_STATS,
				stats);
	} else if (info->attrs[TASKSTATS_CMD_ATTR_TGID]) {
		u32 tgid = nla_get_u32(info->attrs[TASKSTATS_CMD_ATTR_TGID]);
		rc = fill_tgid(tgid, NULL, &stats);
		if (rc < 0)
			goto err;

		na = nla_nest_start(rep_skb, TASKSTATS_TYPE_AGGR_TGID);
		NLA_PUT_U32(rep_skb, TASKSTATS_TYPE_TGID, tgid);
		NLA_PUT_TYPE(rep_skb, struct taskstats, TASKSTATS_TYPE_STATS,
				stats);
	} else {
		rc = -EINVAL;
		goto err;
	}

	nla_nest_end(rep_skb, na);

	return send_reply(rep_skb, info->snd_pid, TASKSTATS_MSG_UNICAST);

nla_put_failure:
	return genlmsg_cancel(rep_skb, reply);
err:
	nlmsg_free(rep_skb);
	return rc;
}

/* Send pid data out on exit */
void taskstats_exit_send(struct task_struct *tsk, struct taskstats *tidstats,
			int group_dead)
{
	int rc;
	struct sk_buff *rep_skb;
	void *reply;
	size_t size;
	int is_thread_group;
	struct nlattr *na;
	unsigned long flags;

	if (!family_registered || !tidstats)
		return;

	spin_lock_irqsave(&tsk->signal->stats_lock, flags);
	is_thread_group = tsk->signal->stats ? 1 : 0;
	spin_unlock_irqrestore(&tsk->signal->stats_lock, flags);

	rc = 0;
	/*
	 * Size includes space for nested attributes
	 */
	size = nla_total_size(sizeof(u32)) +
		nla_total_size(sizeof(struct taskstats)) + nla_total_size(0);

	if (is_thread_group)
		size = 2 * size;	/* PID + STATS + TGID + STATS */

	rc = prepare_reply(NULL, TASKSTATS_CMD_NEW, &rep_skb, &reply, size);
	if (rc < 0)
		goto ret;

	rc = fill_pid(tsk->pid, tsk, tidstats);
	if (rc < 0)
		goto err_skb;

	na = nla_nest_start(rep_skb, TASKSTATS_TYPE_AGGR_PID);
	NLA_PUT_U32(rep_skb, TASKSTATS_TYPE_PID, (u32)tsk->pid);
	NLA_PUT_TYPE(rep_skb, struct taskstats, TASKSTATS_TYPE_STATS,
			*tidstats);
	nla_nest_end(rep_skb, na);

	if (!is_thread_group)
		goto send;

	/*
	 * tsk has/had a thread group so fill the tsk->signal->stats structure
	 * Doesn't matter if tsk is the leader or the last group member leaving
	 */

	fill_tgid_exit(tsk);
	if (!group_dead)
		goto send;

	na = nla_nest_start(rep_skb, TASKSTATS_TYPE_AGGR_TGID);
	NLA_PUT_U32(rep_skb, TASKSTATS_TYPE_TGID, (u32)tsk->tgid);
	/* No locking needed for tsk->signal->stats since group is dead */
	NLA_PUT_TYPE(rep_skb, struct taskstats, TASKSTATS_TYPE_STATS,
			*tsk->signal->stats);
	nla_nest_end(rep_skb, na);

send:
	send_reply(rep_skb, 0, TASKSTATS_MSG_MULTICAST);
	return;

nla_put_failure:
	genlmsg_cancel(rep_skb, reply);
	goto ret;
err_skb:
	nlmsg_free(rep_skb);
ret:
	return;
}

static struct genl_ops taskstats_ops = {
	.cmd		= TASKSTATS_CMD_GET,
	.doit		= taskstats_send_stats,
	.policy		= taskstats_cmd_get_policy,
};

/* Needed early in initialization */
void __init taskstats_init_early(void)
{
	taskstats_cache = kmem_cache_create("taskstats_cache",
						sizeof(struct taskstats),
						0, SLAB_PANIC, NULL, NULL);
}

static int __init taskstats_init(void)
{
	int rc;

	rc = genl_register_family(&family);
	if (rc)
		return rc;

	rc = genl_register_ops(&family, &taskstats_ops);
	if (rc < 0)
		goto err;

	family_registered = 1;
	return 0;
err:
	genl_unregister_family(&family);
	return rc;
}

/*
 * late initcall ensures initialization of statistics collection
 * mechanisms precedes initialization of the taskstats interface
 */
late_initcall(taskstats_init);
