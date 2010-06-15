/*
 * security/tomoyo/domain.c
 *
 * Domain transition functions for TOMOYO.
 *
 * Copyright (C) 2005-2010  NTT DATA CORPORATION
 */

#include "common.h"
#include <linux/binfmts.h>
#include <linux/slab.h>

/* Variables definitions.*/

/* The initial domain. */
struct tomoyo_domain_info tomoyo_kernel_domain;

/**
 * tomoyo_update_domain - Update an entry for domain policy.
 *
 * @new_entry:       Pointer to "struct tomoyo_acl_info".
 * @size:            Size of @new_entry in bytes.
 * @is_delete:       True if it is a delete request.
 * @domain:          Pointer to "struct tomoyo_domain_info".
 * @check_duplicate: Callback function to find duplicated entry.
 * @merge_duplicate: Callback function to merge duplicated entry.
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
int tomoyo_update_domain(struct tomoyo_acl_info *new_entry, const int size,
			 bool is_delete, struct tomoyo_domain_info *domain,
			 bool (*check_duplicate) (const struct tomoyo_acl_info
						  *,
						  const struct tomoyo_acl_info
						  *),
			 bool (*merge_duplicate) (struct tomoyo_acl_info *,
						  struct tomoyo_acl_info *,
						  const bool))
{
	int error = is_delete ? -ENOENT : -ENOMEM;
	struct tomoyo_acl_info *entry;

	if (mutex_lock_interruptible(&tomoyo_policy_lock))
		return error;
	list_for_each_entry_rcu(entry, &domain->acl_info_list, list) {
		if (!check_duplicate(entry, new_entry))
			continue;
		if (merge_duplicate)
			entry->is_deleted = merge_duplicate(entry, new_entry,
							    is_delete);
		else
			entry->is_deleted = is_delete;
		error = 0;
		break;
	}
	if (error && !is_delete) {
		entry = tomoyo_commit_ok(new_entry, size);
		if (entry) {
			list_add_tail_rcu(&entry->list, &domain->acl_info_list);
			error = 0;
		}
	}
	mutex_unlock(&tomoyo_policy_lock);
	return error;
}

/*
 * tomoyo_domain_list is used for holding list of domains.
 * The ->acl_info_list of "struct tomoyo_domain_info" is used for holding
 * permissions (e.g. "allow_read /lib/libc-2.5.so") given to each domain.
 *
 * An entry is added by
 *
 * # ( echo "<kernel>"; echo "allow_execute /sbin/init" ) > \
 *                                  /sys/kernel/security/tomoyo/domain_policy
 *
 * and is deleted by
 *
 * # ( echo "<kernel>"; echo "delete allow_execute /sbin/init" ) > \
 *                                  /sys/kernel/security/tomoyo/domain_policy
 *
 * and all entries are retrieved by
 *
 * # cat /sys/kernel/security/tomoyo/domain_policy
 *
 * A domain is added by
 *
 * # echo "<kernel>" > /sys/kernel/security/tomoyo/domain_policy
 *
 * and is deleted by
 *
 * # echo "delete <kernel>" > /sys/kernel/security/tomoyo/domain_policy
 *
 * and all domains are retrieved by
 *
 * # grep '^<kernel>' /sys/kernel/security/tomoyo/domain_policy
 *
 * Normally, a domainname is monotonically getting longer because a domainname
 * which the process will belong to if an execve() operation succeeds is
 * defined as a concatenation of "current domainname" + "pathname passed to
 * execve()".
 * See tomoyo_domain_initializer_list and tomoyo_domain_keeper_list for
 * exceptions.
 */
LIST_HEAD(tomoyo_domain_list);

/**
 * tomoyo_get_last_name - Get last component of a domainname.
 *
 * @domain: Pointer to "struct tomoyo_domain_info".
 *
 * Returns the last component of the domainname.
 */
const char *tomoyo_get_last_name(const struct tomoyo_domain_info *domain)
{
	const char *cp0 = domain->domainname->name;
	const char *cp1 = strrchr(cp0, ' ');

	if (cp1)
		return cp1 + 1;
	return cp0;
}

/*
 * tomoyo_domain_initializer_list is used for holding list of programs which
 * triggers reinitialization of domainname. Normally, a domainname is
 * monotonically getting longer. But sometimes, we restart daemon programs.
 * It would be convenient for us that "a daemon started upon system boot" and
 * "the daemon restarted from console" belong to the same domain. Thus, TOMOYO
 * provides a way to shorten domainnames.
 *
 * An entry is added by
 *
 * # echo 'initialize_domain /usr/sbin/httpd' > \
 *                               /sys/kernel/security/tomoyo/exception_policy
 *
 * and is deleted by
 *
 * # echo 'delete initialize_domain /usr/sbin/httpd' > \
 *                               /sys/kernel/security/tomoyo/exception_policy
 *
 * and all entries are retrieved by
 *
 * # grep ^initialize_domain /sys/kernel/security/tomoyo/exception_policy
 *
 * In the example above, /usr/sbin/httpd will belong to
 * "<kernel> /usr/sbin/httpd" domain.
 *
 * You may specify a domainname using "from" keyword.
 * "initialize_domain /usr/sbin/httpd from <kernel> /etc/rc.d/init.d/httpd"
 * will cause "/usr/sbin/httpd" executed from "<kernel> /etc/rc.d/init.d/httpd"
 * domain to belong to "<kernel> /usr/sbin/httpd" domain.
 *
 * You may add "no_" prefix to "initialize_domain".
 * "initialize_domain /usr/sbin/httpd" and
 * "no_initialize_domain /usr/sbin/httpd from <kernel> /etc/rc.d/init.d/httpd"
 * will cause "/usr/sbin/httpd" to belong to "<kernel> /usr/sbin/httpd" domain
 * unless executed from "<kernel> /etc/rc.d/init.d/httpd" domain.
 */
LIST_HEAD(tomoyo_domain_initializer_list);

/**
 * tomoyo_update_domain_initializer_entry - Update "struct tomoyo_domain_initializer_entry" list.
 *
 * @domainname: The name of domain. May be NULL.
 * @program:    The name of program.
 * @is_not:     True if it is "no_initialize_domain" entry.
 * @is_delete:  True if it is a delete request.
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
static int tomoyo_update_domain_initializer_entry(const char *domainname,
						  const char *program,
						  const bool is_not,
						  const bool is_delete)
{
	struct tomoyo_domain_initializer_entry *ptr;
	struct tomoyo_domain_initializer_entry e = { .is_not = is_not };
	int error = is_delete ? -ENOENT : -ENOMEM;

	if (!tomoyo_is_correct_path(program))
		return -EINVAL;
	if (domainname) {
		if (!tomoyo_is_domain_def(domainname) &&
		    tomoyo_is_correct_path(domainname))
			e.is_last_name = true;
		else if (!tomoyo_is_correct_domain(domainname))
			return -EINVAL;
		e.domainname = tomoyo_get_name(domainname);
		if (!e.domainname)
			goto out;
	}
	e.program = tomoyo_get_name(program);
	if (!e.program)
		goto out;
	if (mutex_lock_interruptible(&tomoyo_policy_lock))
		goto out;
	list_for_each_entry_rcu(ptr, &tomoyo_domain_initializer_list,
				head.list) {
		if (!tomoyo_is_same_domain_initializer_entry(ptr, &e))
			continue;
		ptr->head.is_deleted = is_delete;
		error = 0;
		break;
	}
	if (!is_delete && error) {
		struct tomoyo_domain_initializer_entry *entry =
			tomoyo_commit_ok(&e, sizeof(e));
		if (entry) {
			list_add_tail_rcu(&entry->head.list,
					  &tomoyo_domain_initializer_list);
			error = 0;
		}
	}
	mutex_unlock(&tomoyo_policy_lock);
 out:
	tomoyo_put_name(e.domainname);
	tomoyo_put_name(e.program);
	return error;
}

/**
 * tomoyo_read_domain_initializer_policy - Read "struct tomoyo_domain_initializer_entry" list.
 *
 * @head: Pointer to "struct tomoyo_io_buffer".
 *
 * Returns true on success, false otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
bool tomoyo_read_domain_initializer_policy(struct tomoyo_io_buffer *head)
{
	struct list_head *pos;
	bool done = true;

	list_for_each_cookie(pos, head->read_var2,
			     &tomoyo_domain_initializer_list) {
		const char *no;
		const char *from = "";
		const char *domain = "";
		struct tomoyo_domain_initializer_entry *ptr;
		ptr = list_entry(pos, struct tomoyo_domain_initializer_entry,
				 head.list);
		if (ptr->head.is_deleted)
			continue;
		no = ptr->is_not ? "no_" : "";
		if (ptr->domainname) {
			from = " from ";
			domain = ptr->domainname->name;
		}
		done = tomoyo_io_printf(head,
					"%s" TOMOYO_KEYWORD_INITIALIZE_DOMAIN
					"%s%s%s\n", no, ptr->program->name,
					from, domain);
		if (!done)
			break;
	}
	return done;
}

/**
 * tomoyo_write_domain_initializer_policy - Write "struct tomoyo_domain_initializer_entry" list.
 *
 * @data:      String to parse.
 * @is_not:    True if it is "no_initialize_domain" entry.
 * @is_delete: True if it is a delete request.
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
int tomoyo_write_domain_initializer_policy(char *data, const bool is_not,
					   const bool is_delete)
{
	char *cp = strstr(data, " from ");

	if (cp) {
		*cp = '\0';
		return tomoyo_update_domain_initializer_entry(cp + 6, data,
							      is_not,
							      is_delete);
	}
	return tomoyo_update_domain_initializer_entry(NULL, data, is_not,
						      is_delete);
}

/**
 * tomoyo_is_domain_initializer - Check whether the given program causes domainname reinitialization.
 *
 * @domainname: The name of domain.
 * @program:    The name of program.
 * @last_name:  The last component of @domainname.
 *
 * Returns true if executing @program reinitializes domain transition,
 * false otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
static bool tomoyo_is_domain_initializer(const struct tomoyo_path_info *
					 domainname,
					 const struct tomoyo_path_info *program,
					 const struct tomoyo_path_info *
					 last_name)
{
	struct tomoyo_domain_initializer_entry *ptr;
	bool flag = false;

	list_for_each_entry_rcu(ptr, &tomoyo_domain_initializer_list,
				head.list) {
		if (ptr->head.is_deleted)
			continue;
		if (ptr->domainname) {
			if (!ptr->is_last_name) {
				if (ptr->domainname != domainname)
					continue;
			} else {
				if (tomoyo_pathcmp(ptr->domainname, last_name))
					continue;
			}
		}
		if (tomoyo_pathcmp(ptr->program, program))
			continue;
		if (ptr->is_not) {
			flag = false;
			break;
		}
		flag = true;
	}
	return flag;
}

/*
 * tomoyo_domain_keeper_list is used for holding list of domainnames which
 * suppresses domain transition. Normally, a domainname is monotonically
 * getting longer. But sometimes, we want to suppress domain transition.
 * It would be convenient for us that programs executed from a login session
 * belong to the same domain. Thus, TOMOYO provides a way to suppress domain
 * transition.
 *
 * An entry is added by
 *
 * # echo 'keep_domain <kernel> /usr/sbin/sshd /bin/bash' > \
 *                              /sys/kernel/security/tomoyo/exception_policy
 *
 * and is deleted by
 *
 * # echo 'delete keep_domain <kernel> /usr/sbin/sshd /bin/bash' > \
 *                              /sys/kernel/security/tomoyo/exception_policy
 *
 * and all entries are retrieved by
 *
 * # grep ^keep_domain /sys/kernel/security/tomoyo/exception_policy
 *
 * In the example above, any process which belongs to
 * "<kernel> /usr/sbin/sshd /bin/bash" domain will remain in that domain,
 * unless explicitly specified by "initialize_domain" or "no_keep_domain".
 *
 * You may specify a program using "from" keyword.
 * "keep_domain /bin/pwd from <kernel> /usr/sbin/sshd /bin/bash"
 * will cause "/bin/pwd" executed from "<kernel> /usr/sbin/sshd /bin/bash"
 * domain to remain in "<kernel> /usr/sbin/sshd /bin/bash" domain.
 *
 * You may add "no_" prefix to "keep_domain".
 * "keep_domain <kernel> /usr/sbin/sshd /bin/bash" and
 * "no_keep_domain /usr/bin/passwd from <kernel> /usr/sbin/sshd /bin/bash" will
 * cause "/usr/bin/passwd" to belong to
 * "<kernel> /usr/sbin/sshd /bin/bash /usr/bin/passwd" domain, unless
 * explicitly specified by "initialize_domain".
 */
LIST_HEAD(tomoyo_domain_keeper_list);

/**
 * tomoyo_update_domain_keeper_entry - Update "struct tomoyo_domain_keeper_entry" list.
 *
 * @domainname: The name of domain.
 * @program:    The name of program. May be NULL.
 * @is_not:     True if it is "no_keep_domain" entry.
 * @is_delete:  True if it is a delete request.
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
static int tomoyo_update_domain_keeper_entry(const char *domainname,
					     const char *program,
					     const bool is_not,
					     const bool is_delete)
{
	struct tomoyo_domain_keeper_entry *ptr;
	struct tomoyo_domain_keeper_entry e = { .is_not = is_not };
	int error = is_delete ? -ENOENT : -ENOMEM;

	if (!tomoyo_is_domain_def(domainname) &&
	    tomoyo_is_correct_path(domainname))
		e.is_last_name = true;
	else if (!tomoyo_is_correct_domain(domainname))
		return -EINVAL;
	if (program) {
		if (!tomoyo_is_correct_path(program))
			return -EINVAL;
		e.program = tomoyo_get_name(program);
		if (!e.program)
			goto out;
	}
	e.domainname = tomoyo_get_name(domainname);
	if (!e.domainname)
		goto out;
	if (mutex_lock_interruptible(&tomoyo_policy_lock))
		goto out;
	list_for_each_entry_rcu(ptr, &tomoyo_domain_keeper_list, head.list) {
		if (!tomoyo_is_same_domain_keeper_entry(ptr, &e))
			continue;
		ptr->head.is_deleted = is_delete;
		error = 0;
		break;
	}
	if (!is_delete && error) {
		struct tomoyo_domain_keeper_entry *entry =
			tomoyo_commit_ok(&e, sizeof(e));
		if (entry) {
			list_add_tail_rcu(&entry->head.list,
					  &tomoyo_domain_keeper_list);
			error = 0;
		}
	}
	mutex_unlock(&tomoyo_policy_lock);
 out:
	tomoyo_put_name(e.domainname);
	tomoyo_put_name(e.program);
	return error;
}

/**
 * tomoyo_write_domain_keeper_policy - Write "struct tomoyo_domain_keeper_entry" list.
 *
 * @data:      String to parse.
 * @is_not:    True if it is "no_keep_domain" entry.
 * @is_delete: True if it is a delete request.
 *
 * Caller holds tomoyo_read_lock().
 */
int tomoyo_write_domain_keeper_policy(char *data, const bool is_not,
				      const bool is_delete)
{
	char *cp = strstr(data, " from ");

	if (cp) {
		*cp = '\0';
		return tomoyo_update_domain_keeper_entry(cp + 6, data, is_not,
							 is_delete);
	}
	return tomoyo_update_domain_keeper_entry(data, NULL, is_not, is_delete);
}

/**
 * tomoyo_read_domain_keeper_policy - Read "struct tomoyo_domain_keeper_entry" list.
 *
 * @head: Pointer to "struct tomoyo_io_buffer".
 *
 * Returns true on success, false otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
bool tomoyo_read_domain_keeper_policy(struct tomoyo_io_buffer *head)
{
	struct list_head *pos;
	bool done = true;

	list_for_each_cookie(pos, head->read_var2,
			     &tomoyo_domain_keeper_list) {
		struct tomoyo_domain_keeper_entry *ptr;
		const char *no;
		const char *from = "";
		const char *program = "";

		ptr = list_entry(pos, struct tomoyo_domain_keeper_entry,
				 head.list);
		if (ptr->head.is_deleted)
			continue;
		no = ptr->is_not ? "no_" : "";
		if (ptr->program) {
			from = " from ";
			program = ptr->program->name;
		}
		done = tomoyo_io_printf(head,
					"%s" TOMOYO_KEYWORD_KEEP_DOMAIN
					"%s%s%s\n", no, program, from,
					ptr->domainname->name);
		if (!done)
			break;
	}
	return done;
}

/**
 * tomoyo_is_domain_keeper - Check whether the given program causes domain transition suppression.
 *
 * @domainname: The name of domain.
 * @program:    The name of program.
 * @last_name:  The last component of @domainname.
 *
 * Returns true if executing @program supresses domain transition,
 * false otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
static bool tomoyo_is_domain_keeper(const struct tomoyo_path_info *domainname,
				    const struct tomoyo_path_info *program,
				    const struct tomoyo_path_info *last_name)
{
	struct tomoyo_domain_keeper_entry *ptr;
	bool flag = false;

	list_for_each_entry_rcu(ptr, &tomoyo_domain_keeper_list, head.list) {
		if (ptr->head.is_deleted)
			continue;
		if (!ptr->is_last_name) {
			if (ptr->domainname != domainname)
				continue;
		} else {
			if (tomoyo_pathcmp(ptr->domainname, last_name))
				continue;
		}
		if (ptr->program && tomoyo_pathcmp(ptr->program, program))
			continue;
		if (ptr->is_not) {
			flag = false;
			break;
		}
		flag = true;
	}
	return flag;
}

/*
 * tomoyo_aggregator_list is used for holding list of rewrite table for
 * execve() request. Some programs provides similar functionality. This keyword
 * allows users to aggregate such programs.
 *
 * Entries are added by
 *
 * # echo 'aggregator /usr/bin/vi /./editor' > \
 *                            /sys/kernel/security/tomoyo/exception_policy
 * # echo 'aggregator /usr/bin/emacs /./editor' > \
 *                            /sys/kernel/security/tomoyo/exception_policy
 *
 * and are deleted by
 *
 * # echo 'delete aggregator /usr/bin/vi /./editor' > \
 *                            /sys/kernel/security/tomoyo/exception_policy
 * # echo 'delete aggregator /usr/bin/emacs /./editor' > \
 *                            /sys/kernel/security/tomoyo/exception_policy
 *
 * and all entries are retrieved by
 *
 * # grep ^aggregator /sys/kernel/security/tomoyo/exception_policy
 *
 * In the example above, if /usr/bin/vi or /usr/bin/emacs are executed,
 * permission is checked for /./editor and domainname which the current process
 * will belong to after execve() succeeds is calculated using /./editor .
 */
LIST_HEAD(tomoyo_aggregator_list);

/**
 * tomoyo_update_aggregator_entry - Update "struct tomoyo_aggregator_entry" list.
 *
 * @original_name:   The original program's name.
 * @aggregated_name: The program name to use.
 * @is_delete:       True if it is a delete request.
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
static int tomoyo_update_aggregator_entry(const char *original_name,
					  const char *aggregated_name,
					  const bool is_delete)
{
	struct tomoyo_aggregator_entry *ptr;
	struct tomoyo_aggregator_entry e = { };
	int error = is_delete ? -ENOENT : -ENOMEM;

	if (!tomoyo_is_correct_path(original_name) ||
	    !tomoyo_is_correct_path(aggregated_name))
		return -EINVAL;
	e.original_name = tomoyo_get_name(original_name);
	e.aggregated_name = tomoyo_get_name(aggregated_name);
	if (!e.original_name || !e.aggregated_name ||
	    e.aggregated_name->is_patterned) /* No patterns allowed. */
		goto out;
	if (mutex_lock_interruptible(&tomoyo_policy_lock))
		goto out;
	list_for_each_entry_rcu(ptr, &tomoyo_aggregator_list, head.list) {
		if (!tomoyo_is_same_aggregator_entry(ptr, &e))
			continue;
		ptr->head.is_deleted = is_delete;
		error = 0;
		break;
	}
	if (!is_delete && error) {
		struct tomoyo_aggregator_entry *entry =
			tomoyo_commit_ok(&e, sizeof(e));
		if (entry) {
			list_add_tail_rcu(&entry->head.list,
					  &tomoyo_aggregator_list);
			error = 0;
		}
	}
	mutex_unlock(&tomoyo_policy_lock);
 out:
	tomoyo_put_name(e.original_name);
	tomoyo_put_name(e.aggregated_name);
	return error;
}

/**
 * tomoyo_read_aggregator_policy - Read "struct tomoyo_aggregator_entry" list.
 *
 * @head: Pointer to "struct tomoyo_io_buffer".
 *
 * Returns true on success, false otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
bool tomoyo_read_aggregator_policy(struct tomoyo_io_buffer *head)
{
	struct list_head *pos;
	bool done = true;

	list_for_each_cookie(pos, head->read_var2, &tomoyo_aggregator_list) {
		struct tomoyo_aggregator_entry *ptr;

		ptr = list_entry(pos, struct tomoyo_aggregator_entry,
				 head.list);
		if (ptr->head.is_deleted)
			continue;
		done = tomoyo_io_printf(head, TOMOYO_KEYWORD_AGGREGATOR
					"%s %s\n", ptr->original_name->name,
					ptr->aggregated_name->name);
		if (!done)
			break;
	}
	return done;
}

/**
 * tomoyo_write_aggregator_policy - Write "struct tomoyo_aggregator_entry" list.
 *
 * @data:      String to parse.
 * @is_delete: True if it is a delete request.
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
int tomoyo_write_aggregator_policy(char *data, const bool is_delete)
{
	char *cp = strchr(data, ' ');

	if (!cp)
		return -EINVAL;
	*cp++ = '\0';
	return tomoyo_update_aggregator_entry(data, cp, is_delete);
}

/*
 * tomoyo_alias_list is used for holding list of symlink's pathnames which are
 * allowed to be passed to an execve() request. Normally, the domainname which
 * the current process will belong to after execve() succeeds is calculated
 * using dereferenced pathnames. But some programs behave differently depending
 * on the name passed to argv[0]. For busybox, calculating domainname using
 * dereferenced pathnames will cause all programs in the busybox to belong to
 * the same domain. Thus, TOMOYO provides a way to allow use of symlink's
 * pathname for checking execve()'s permission and calculating domainname which
 * the current process will belong to after execve() succeeds.
 *
 * An entry is added by
 *
 * # echo 'alias /bin/busybox /bin/cat' > \
 *                            /sys/kernel/security/tomoyo/exception_policy
 *
 * and is deleted by
 *
 * # echo 'delete alias /bin/busybox /bin/cat' > \
 *                            /sys/kernel/security/tomoyo/exception_policy
 *
 * and all entries are retrieved by
 *
 * # grep ^alias /sys/kernel/security/tomoyo/exception_policy
 *
 * In the example above, if /bin/cat is a symlink to /bin/busybox and execution
 * of /bin/cat is requested, permission is checked for /bin/cat rather than
 * /bin/busybox and domainname which the current process will belong to after
 * execve() succeeds is calculated using /bin/cat rather than /bin/busybox .
 */
LIST_HEAD(tomoyo_alias_list);

/**
 * tomoyo_update_alias_entry - Update "struct tomoyo_alias_entry" list.
 *
 * @original_name: The original program's real name.
 * @aliased_name:  The symbolic program's symbolic link's name.
 * @is_delete:     True if it is a delete request.
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
static int tomoyo_update_alias_entry(const char *original_name,
				     const char *aliased_name,
				     const bool is_delete)
{
	struct tomoyo_alias_entry *ptr;
	struct tomoyo_alias_entry e = { };
	int error = is_delete ? -ENOENT : -ENOMEM;

	if (!tomoyo_is_correct_path(original_name) ||
	    !tomoyo_is_correct_path(aliased_name))
		return -EINVAL;
	e.original_name = tomoyo_get_name(original_name);
	e.aliased_name = tomoyo_get_name(aliased_name);
	if (!e.original_name || !e.aliased_name ||
	    e.original_name->is_patterned || e.aliased_name->is_patterned)
		goto out; /* No patterns allowed. */
	if (mutex_lock_interruptible(&tomoyo_policy_lock))
		goto out;
	list_for_each_entry_rcu(ptr, &tomoyo_alias_list, head.list) {
		if (!tomoyo_is_same_alias_entry(ptr, &e))
			continue;
		ptr->head.is_deleted = is_delete;
		error = 0;
		break;
	}
	if (!is_delete && error) {
		struct tomoyo_alias_entry *entry =
			tomoyo_commit_ok(&e, sizeof(e));
		if (entry) {
			list_add_tail_rcu(&entry->head.list,
					  &tomoyo_alias_list);
			error = 0;
		}
	}
	mutex_unlock(&tomoyo_policy_lock);
 out:
	tomoyo_put_name(e.original_name);
	tomoyo_put_name(e.aliased_name);
	return error;
}

/**
 * tomoyo_read_alias_policy - Read "struct tomoyo_alias_entry" list.
 *
 * @head: Pointer to "struct tomoyo_io_buffer".
 *
 * Returns true on success, false otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
bool tomoyo_read_alias_policy(struct tomoyo_io_buffer *head)
{
	struct list_head *pos;
	bool done = true;

	list_for_each_cookie(pos, head->read_var2, &tomoyo_alias_list) {
		struct tomoyo_alias_entry *ptr;

		ptr = list_entry(pos, struct tomoyo_alias_entry, head.list);
		if (ptr->head.is_deleted)
			continue;
		done = tomoyo_io_printf(head, TOMOYO_KEYWORD_ALIAS "%s %s\n",
					ptr->original_name->name,
					ptr->aliased_name->name);
		if (!done)
			break;
	}
	return done;
}

/**
 * tomoyo_write_alias_policy - Write "struct tomoyo_alias_entry" list.
 *
 * @data:      String to parse.
 * @is_delete: True if it is a delete request.
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
int tomoyo_write_alias_policy(char *data, const bool is_delete)
{
	char *cp = strchr(data, ' ');

	if (!cp)
		return -EINVAL;
	*cp++ = '\0';
	return tomoyo_update_alias_entry(data, cp, is_delete);
}

/**
 * tomoyo_find_or_assign_new_domain - Create a domain.
 *
 * @domainname: The name of domain.
 * @profile:    Profile number to assign if the domain was newly created.
 *
 * Returns pointer to "struct tomoyo_domain_info" on success, NULL otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
struct tomoyo_domain_info *tomoyo_find_or_assign_new_domain(const char *
							    domainname,
							    const u8 profile)
{
	struct tomoyo_domain_info *entry;
	struct tomoyo_domain_info *domain = NULL;
	const struct tomoyo_path_info *saved_domainname;
	bool found = false;

	if (!tomoyo_is_correct_domain(domainname))
		return NULL;
	saved_domainname = tomoyo_get_name(domainname);
	if (!saved_domainname)
		return NULL;
	entry = kzalloc(sizeof(*entry), GFP_NOFS);
	if (mutex_lock_interruptible(&tomoyo_policy_lock))
		goto out;
	list_for_each_entry_rcu(domain, &tomoyo_domain_list, list) {
		if (domain->is_deleted ||
		    tomoyo_pathcmp(saved_domainname, domain->domainname))
			continue;
		found = true;
		break;
	}
	if (!found && tomoyo_memory_ok(entry)) {
		INIT_LIST_HEAD(&entry->acl_info_list);
		entry->domainname = saved_domainname;
		saved_domainname = NULL;
		entry->profile = profile;
		list_add_tail_rcu(&entry->list, &tomoyo_domain_list);
		domain = entry;
		entry = NULL;
		found = true;
	}
	mutex_unlock(&tomoyo_policy_lock);
 out:
	tomoyo_put_name(saved_domainname);
	kfree(entry);
	return found ? domain : NULL;
}

/**
 * tomoyo_find_next_domain - Find a domain.
 *
 * @bprm: Pointer to "struct linux_binprm".
 *
 * Returns 0 on success, negative value otherwise.
 *
 * Caller holds tomoyo_read_lock().
 */
int tomoyo_find_next_domain(struct linux_binprm *bprm)
{
	struct tomoyo_request_info r;
	char *tmp = kzalloc(TOMOYO_EXEC_TMPSIZE, GFP_NOFS);
	struct tomoyo_domain_info *old_domain = tomoyo_domain();
	struct tomoyo_domain_info *domain = NULL;
	const char *old_domain_name = old_domain->domainname->name;
	const char *original_name = bprm->filename;
	u8 mode;
	bool is_enforce;
	int retval = -ENOMEM;
	bool need_kfree = false;
	struct tomoyo_path_info rn = { }; /* real name */
	struct tomoyo_path_info sn = { }; /* symlink name */
	struct tomoyo_path_info ln; /* last name */

	ln.name = tomoyo_get_last_name(old_domain);
	tomoyo_fill_path_info(&ln);
	mode = tomoyo_init_request_info(&r, NULL, TOMOYO_MAC_FILE_EXECUTE);
	is_enforce = (mode == TOMOYO_CONFIG_ENFORCING);
	if (!tmp)
		goto out;

 retry:
	if (need_kfree) {
		kfree(rn.name);
		need_kfree = false;
	}
	/* Get tomoyo_realpath of program. */
	retval = -ENOENT;
	rn.name = tomoyo_realpath(original_name);
	if (!rn.name)
		goto out;
	tomoyo_fill_path_info(&rn);
	need_kfree = true;

	/* Get tomoyo_realpath of symbolic link. */
	sn.name = tomoyo_realpath_nofollow(original_name);
	if (!sn.name)
		goto out;
	tomoyo_fill_path_info(&sn);

	/* Check 'alias' directive. */
	if (tomoyo_pathcmp(&rn, &sn)) {
		struct tomoyo_alias_entry *ptr;
		/* Is this program allowed to be called via symbolic links? */
		list_for_each_entry_rcu(ptr, &tomoyo_alias_list, head.list) {
			if (ptr->head.is_deleted ||
			    tomoyo_pathcmp(&rn, ptr->original_name) ||
			    tomoyo_pathcmp(&sn, ptr->aliased_name))
				continue;
			kfree(rn.name);
			need_kfree = false;
			/* This is OK because it is read only. */
			rn = *ptr->aliased_name;
			break;
		}
	}

	/* Check 'aggregator' directive. */
	{
		struct tomoyo_aggregator_entry *ptr;
		list_for_each_entry_rcu(ptr, &tomoyo_aggregator_list,
					head.list) {
			if (ptr->head.is_deleted ||
			    !tomoyo_path_matches_pattern(&rn,
							 ptr->original_name))
				continue;
			if (need_kfree)
				kfree(rn.name);
			need_kfree = false;
			/* This is OK because it is read only. */
			rn = *ptr->aggregated_name;
			break;
		}
	}

	/* Check execute permission. */
	retval = tomoyo_check_exec_perm(&r, &rn);
	if (retval == TOMOYO_RETRY_REQUEST)
		goto retry;
	if (retval < 0)
		goto out;

	if (tomoyo_is_domain_initializer(old_domain->domainname, &rn, &ln)) {
		/* Transit to the child of tomoyo_kernel_domain domain. */
		snprintf(tmp, TOMOYO_EXEC_TMPSIZE - 1,
			 TOMOYO_ROOT_NAME " " "%s", rn.name);
	} else if (old_domain == &tomoyo_kernel_domain &&
		   !tomoyo_policy_loaded) {
		/*
		 * Needn't to transit from kernel domain before starting
		 * /sbin/init. But transit from kernel domain if executing
		 * initializers because they might start before /sbin/init.
		 */
		domain = old_domain;
	} else if (tomoyo_is_domain_keeper(old_domain->domainname, &rn, &ln)) {
		/* Keep current domain. */
		domain = old_domain;
	} else {
		/* Normal domain transition. */
		snprintf(tmp, TOMOYO_EXEC_TMPSIZE - 1,
			 "%s %s", old_domain_name, rn.name);
	}
	if (domain || strlen(tmp) >= TOMOYO_EXEC_TMPSIZE - 10)
		goto done;
	domain = tomoyo_find_domain(tmp);
	if (domain)
		goto done;
	if (is_enforce) {
		int error = tomoyo_supervisor(&r, "# wants to create domain\n"
					      "%s\n", tmp);
		if (error == TOMOYO_RETRY_REQUEST)
			goto retry;
		if (error < 0)
			goto done;
	}
	domain = tomoyo_find_or_assign_new_domain(tmp, old_domain->profile);
 done:
	if (domain)
		goto out;
	printk(KERN_WARNING "TOMOYO-ERROR: Domain '%s' not defined.\n", tmp);
	if (is_enforce)
		retval = -EPERM;
	else
		old_domain->transition_failed = true;
 out:
	if (!domain)
		domain = old_domain;
	/* Update reference count on "struct tomoyo_domain_info". */
	atomic_inc(&domain->users);
	bprm->cred->security = domain;
	if (need_kfree)
		kfree(rn.name);
	kfree(sn.name);
	kfree(tmp);
	return retval;
}
