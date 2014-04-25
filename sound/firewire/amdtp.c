/*
 * Audio and Music Data Transmission Protocol (IEC 61883-6) streams
 * with Common Isochronous Packet (IEC 61883-1) headers
 *
 * Copyright (c) Clemens Ladisch <clemens@ladisch.de>
 * Licensed under the terms of the GNU General Public License, version 2.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/firewire.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <sound/pcm.h>
#include <sound/rawmidi.h>
#include "amdtp.h"

#define TICKS_PER_CYCLE		3072
#define CYCLES_PER_SECOND	8000
#define TICKS_PER_SECOND	(TICKS_PER_CYCLE * CYCLES_PER_SECOND)

#define TRANSFER_DELAY_TICKS	0x2e00 /* 479.17 µs */

/* isochronous header parameters */
#define ISO_DATA_LENGTH_SHIFT	16
#define TAG_CIP			1

/* common isochronous packet header parameters */
#define CIP_EOH			(1u << 31)
#define CIP_EOH_MASK		0x80000000
#define CIP_FMT_AM		(0x10 << 24)
#define CIP_FMT_MASK		0x3f000000
#define CIP_SYT_MASK		0x0000ffff
#define CIP_SYT_NO_INFO		0xffff
#define CIP_FDF_MASK		0x00ff0000
#define CIP_FDF_SFC_SHIFT	16

/*
 * Audio and Music transfer protocol specific parameters
 * only "Clock-based rate control mode" is supported
 */
#define AMDTP_FDF_AM824		(0 << (CIP_FDF_SFC_SHIFT + 3))
#define AMDTP_FDF_NO_DATA	0xff
#define AMDTP_DBS_MASK		0x00ff0000
#define AMDTP_DBS_SHIFT		16
#define AMDTP_DBC_MASK		0x000000ff

/* TODO: make these configurable */
#define INTERRUPT_INTERVAL	16
#define QUEUE_LENGTH		48

#define IN_PACKET_HEADER_SIZE	4
#define OUT_PACKET_HEADER_SIZE	0

static void pcm_period_tasklet(unsigned long data);

/**
 * amdtp_stream_init - initialize an AMDTP stream structure
 * @s: the AMDTP stream to initialize
 * @unit: the target of the stream
 * @dir: the direction of stream
 * @flags: the packet transmission method to use
 */
int amdtp_stream_init(struct amdtp_stream *s, struct fw_unit *unit,
		      enum amdtp_stream_direction dir, enum cip_flags flags)
{
	s->unit = fw_unit_get(unit);
	s->direction = dir;
	s->flags = flags;
	s->context = ERR_PTR(-1);
	mutex_init(&s->mutex);
	tasklet_init(&s->period_tasklet, pcm_period_tasklet, (unsigned long)s);
	s->packet_index = 0;

	init_waitqueue_head(&s->callback_wait);
	s->callbacked = false;
	s->sync_slave = NULL;

	return 0;
}
EXPORT_SYMBOL(amdtp_stream_init);

/**
 * amdtp_stream_destroy - free stream resources
 * @s: the AMDTP stream to destroy
 */
void amdtp_stream_destroy(struct amdtp_stream *s)
{
	WARN_ON(amdtp_stream_running(s));
	mutex_destroy(&s->mutex);
	fw_unit_put(s->unit);
}
EXPORT_SYMBOL(amdtp_stream_destroy);

const unsigned int amdtp_syt_intervals[CIP_SFC_COUNT] = {
	[CIP_SFC_32000]  =  8,
	[CIP_SFC_44100]  =  8,
	[CIP_SFC_48000]  =  8,
	[CIP_SFC_88200]  = 16,
	[CIP_SFC_96000]  = 16,
	[CIP_SFC_176400] = 32,
	[CIP_SFC_192000] = 32,
};
EXPORT_SYMBOL(amdtp_syt_intervals);

/**
 * amdtp_stream_set_parameters - set stream parameters
 * @s: the AMDTP stream to configure
 * @rate: the sample rate
 * @pcm_channels: the number of PCM samples in each data block, to be encoded
 *                as AM824 multi-bit linear audio
 * @midi_ports: the number of MIDI ports (i.e., MPX-MIDI Data Channels)
 *
 * The parameters must be set before the stream is started, and must not be
 * changed while the stream is running.
 */
void amdtp_stream_set_parameters(struct amdtp_stream *s,
				 unsigned int rate,
				 unsigned int pcm_channels,
				 unsigned int midi_ports)
{
	static const unsigned int rates[] = {
		[CIP_SFC_32000]  =  32000,
		[CIP_SFC_44100]  =  44100,
		[CIP_SFC_48000]  =  48000,
		[CIP_SFC_88200]  =  88200,
		[CIP_SFC_96000]  =  96000,
		[CIP_SFC_176400] = 176400,
		[CIP_SFC_192000] = 192000,
	};
	unsigned int sfc, midi_channels;

	midi_channels = DIV_ROUND_UP(midi_ports, 8);

	if (WARN_ON(amdtp_stream_running(s)) ||
	    WARN_ON(midi_channels > AMDTP_MAX_CHANNELS_FOR_MIDI))
		return;

	for (sfc = 0; sfc < CIP_SFC_COUNT; ++sfc)
		if (rates[sfc] == rate)
			goto sfc_found;
	WARN_ON(1);
	return;

sfc_found:
	s->dual_wire = (s->flags & CIP_HI_DUALWIRE) && sfc > CIP_SFC_96000;
	if (s->dual_wire) {
		sfc -= 2;
		rate /= 2;
		pcm_channels *= 2;
	}
	s->sfc = sfc;
	s->data_block_quadlets = pcm_channels + midi_channels;
	s->pcm_channels = pcm_channels;
	s->midi_ports = midi_ports;

	s->syt_interval = amdtp_syt_intervals[sfc];

	/* default buffering in the device */
	s->transfer_delay = TRANSFER_DELAY_TICKS - TICKS_PER_CYCLE;
	if (s->flags & CIP_BLOCKING)
		/* additional buffering needed to adjust for no-data packets */
		s->transfer_delay += TICKS_PER_SECOND * s->syt_interval / rate;
}
EXPORT_SYMBOL(amdtp_stream_set_parameters);

/**
 * amdtp_stream_get_max_payload - get the stream's packet size
 * @s: the AMDTP stream
 *
 * This function must not be called before the stream has been configured
 * with amdtp_stream_set_parameters().
 */
unsigned int amdtp_stream_get_max_payload(struct amdtp_stream *s)
{
	return 8 + s->syt_interval * s->data_block_quadlets * 4;
}
EXPORT_SYMBOL(amdtp_stream_get_max_payload);

static void amdtp_write_s16(struct amdtp_stream *s,
			    struct snd_pcm_substream *pcm,
			    __be32 *buffer, unsigned int frames);
static void amdtp_write_s32(struct amdtp_stream *s,
			    struct snd_pcm_substream *pcm,
			    __be32 *buffer, unsigned int frames);
static void amdtp_write_s16_dualwire(struct amdtp_stream *s,
				     struct snd_pcm_substream *pcm,
				     __be32 *buffer, unsigned int frames);
static void amdtp_write_s32_dualwire(struct amdtp_stream *s,
				     struct snd_pcm_substream *pcm,
				     __be32 *buffer, unsigned int frames);
static void amdtp_read_s32(struct amdtp_stream *s,
			   struct snd_pcm_substream *pcm,
			   __be32 *buffer, unsigned int frames);
static void amdtp_read_s32_dualwire(struct amdtp_stream *s,
				    struct snd_pcm_substream *pcm,
				    __be32 *buffer, unsigned int frames);

/**
 * amdtp_stream_set_pcm_format - set the PCM format
 * @s: the AMDTP stream to configure
 * @format: the format of the ALSA PCM device
 *
 * The sample format must be set after the other paramters (rate/PCM channels/
 * MIDI) and before the stream is started, and must not be changed while the
 * stream is running.
 */
void amdtp_stream_set_pcm_format(struct amdtp_stream *s,
				 snd_pcm_format_t format)
{
	if (WARN_ON(amdtp_stream_pcm_running(s)))
		return;

	switch (format) {
	default:
		WARN_ON(1);
		/* fall through */
	case SNDRV_PCM_FORMAT_S16:
		if (s->direction == AMDTP_OUT_STREAM) {
			if (s->dual_wire)
				s->transfer_samples = amdtp_write_s16_dualwire;
			else
				s->transfer_samples = amdtp_write_s16;
			break;
		}
		WARN_ON(1);
		/* fall through */
	case SNDRV_PCM_FORMAT_S32:
		if (s->direction == AMDTP_OUT_STREAM) {
			if (s->dual_wire)
				s->transfer_samples = amdtp_write_s32_dualwire;
			else
				s->transfer_samples = amdtp_write_s32;
		} else {
			if (s->dual_wire)
				s->transfer_samples = amdtp_read_s32_dualwire;
			else
				s->transfer_samples = amdtp_read_s32;
		}
		break;
	}
}
EXPORT_SYMBOL(amdtp_stream_set_pcm_format);

/**
 * amdtp_stream_pcm_prepare - prepare PCM device for running
 * @s: the AMDTP stream
 *
 * This function should be called from the PCM device's .prepare callback.
 */
void amdtp_stream_pcm_prepare(struct amdtp_stream *s)
{
	tasklet_kill(&s->period_tasklet);
	s->pcm_buffer_pointer = 0;
	s->pcm_period_pointer = 0;
	s->pointer_flush = true;
}
EXPORT_SYMBOL(amdtp_stream_pcm_prepare);

static unsigned int calculate_data_blocks(struct amdtp_stream *s)
{
	unsigned int phase, data_blocks;

	if (s->flags & CIP_BLOCKING)
		data_blocks = s->syt_interval;
	else if (!cip_sfc_is_base_44100(s->sfc)) {
		/* Sample_rate / 8000 is an integer, and precomputed. */
		data_blocks = s->data_block_state;
	} else {
		phase = s->data_block_state;

		/*
		 * This calculates the number of data blocks per packet so that
		 * 1) the overall rate is correct and exactly synchronized to
		 *    the bus clock, and
		 * 2) packets with a rounded-up number of blocks occur as early
		 *    as possible in the sequence (to prevent underruns of the
		 *    device's buffer).
		 */
		if (s->sfc == CIP_SFC_44100)
			/* 6 6 5 6 5 6 5 ... */
			data_blocks = 5 + ((phase & 1) ^
					   (phase == 0 || phase >= 40));
		else
			/* 12 11 11 11 11 ... or 23 22 22 22 22 ... */
			data_blocks = 11 * (s->sfc >> 1) + (phase == 0);
		if (++phase >= (80 >> (s->sfc >> 1)))
			phase = 0;
		s->data_block_state = phase;
	}

	return data_blocks;
}

static unsigned int calculate_syt(struct amdtp_stream *s,
				  unsigned int cycle)
{
	unsigned int syt_offset, phase, index, syt;

	if (s->last_syt_offset < TICKS_PER_CYCLE) {
		if (!cip_sfc_is_base_44100(s->sfc))
			syt_offset = s->last_syt_offset + s->syt_offset_state;
		else {
		/*
		 * The time, in ticks, of the n'th SYT_INTERVAL sample is:
		 *   n * SYT_INTERVAL * 24576000 / sample_rate
		 * Modulo TICKS_PER_CYCLE, the difference between successive
		 * elements is about 1386.23.  Rounding the results of this
		 * formula to the SYT precision results in a sequence of
		 * differences that begins with:
		 *   1386 1386 1387 1386 1386 1386 1387 1386 1386 1386 1387 ...
		 * This code generates _exactly_ the same sequence.
		 */
			phase = s->syt_offset_state;
			index = phase % 13;
			syt_offset = s->last_syt_offset;
			syt_offset += 1386 + ((index && !(index & 3)) ||
					      phase == 146);
			if (++phase >= 147)
				phase = 0;
			s->syt_offset_state = phase;
		}
	} else
		syt_offset = s->last_syt_offset - TICKS_PER_CYCLE;
	s->last_syt_offset = syt_offset;

	if (syt_offset < TICKS_PER_CYCLE) {
		syt_offset += s->transfer_delay;
		syt = (cycle + syt_offset / TICKS_PER_CYCLE) << 12;
		syt += syt_offset % TICKS_PER_CYCLE;

		return syt & CIP_SYT_MASK;
	} else {
		return CIP_SYT_NO_INFO;
	}
}

static void amdtp_write_s32(struct amdtp_stream *s,
			    struct snd_pcm_substream *pcm,
			    __be32 *buffer, unsigned int frames)
{
	struct snd_pcm_runtime *runtime = pcm->runtime;
	unsigned int channels, remaining_frames, frame_step, i, c;
	const u32 *src;

	channels = s->pcm_channels;
	src = (void *)runtime->dma_area +
			frames_to_bytes(runtime, s->pcm_buffer_pointer);
	remaining_frames = runtime->buffer_size - s->pcm_buffer_pointer;
	frame_step = s->data_block_quadlets - channels;

	for (i = 0; i < frames; ++i) {
		for (c = 0; c < channels; ++c) {
			*buffer = cpu_to_be32((*src >> 8) | 0x40000000);
			src++;
			buffer++;
		}
		buffer += frame_step;
		if (--remaining_frames == 0)
			src = (void *)runtime->dma_area;
	}
}

static void amdtp_write_s16(struct amdtp_stream *s,
			    struct snd_pcm_substream *pcm,
			    __be32 *buffer, unsigned int frames)
{
	struct snd_pcm_runtime *runtime = pcm->runtime;
	unsigned int channels, remaining_frames, frame_step, i, c;
	const u16 *src;

	channels = s->pcm_channels;
	src = (void *)runtime->dma_area +
			frames_to_bytes(runtime, s->pcm_buffer_pointer);
	remaining_frames = runtime->buffer_size - s->pcm_buffer_pointer;
	frame_step = s->data_block_quadlets - channels;

	for (i = 0; i < frames; ++i) {
		for (c = 0; c < channels; ++c) {
			*buffer = cpu_to_be32((*src << 8) | 0x40000000);
			src++;
			buffer++;
		}
		buffer += frame_step;
		if (--remaining_frames == 0)
			src = (void *)runtime->dma_area;
	}
}

static void amdtp_write_s32_dualwire(struct amdtp_stream *s,
				     struct snd_pcm_substream *pcm,
				     __be32 *buffer, unsigned int frames)
{
	struct snd_pcm_runtime *runtime = pcm->runtime;
	unsigned int channels, frame_adjust_1, frame_adjust_2, i, c;
	const u32 *src;

	channels = s->pcm_channels;
	src = (void *)runtime->dma_area +
			s->pcm_buffer_pointer * (runtime->frame_bits / 8);
	frame_adjust_1 = channels - 1;
	frame_adjust_2 = 1 - (s->data_block_quadlets - channels);

	channels /= 2;
	for (i = 0; i < frames; ++i) {
		for (c = 0; c < channels; ++c) {
			*buffer = cpu_to_be32((*src >> 8) | 0x40000000);
			src++;
			buffer += 2;
		}
		buffer -= frame_adjust_1;
		for (c = 0; c < channels; ++c) {
			*buffer = cpu_to_be32((*src >> 8) | 0x40000000);
			src++;
			buffer += 2;
		}
		buffer -= frame_adjust_2;
	}
}

static void amdtp_write_s16_dualwire(struct amdtp_stream *s,
				     struct snd_pcm_substream *pcm,
				     __be32 *buffer, unsigned int frames)
{
	struct snd_pcm_runtime *runtime = pcm->runtime;
	unsigned int channels, frame_adjust_1, frame_adjust_2, i, c;
	const u16 *src;

	channels = s->pcm_channels;
	src = (void *)runtime->dma_area +
			s->pcm_buffer_pointer * (runtime->frame_bits / 8);
	frame_adjust_1 = channels - 1;
	frame_adjust_2 = 1 - (s->data_block_quadlets - channels);

	channels /= 2;
	for (i = 0; i < frames; ++i) {
		for (c = 0; c < channels; ++c) {
			*buffer = cpu_to_be32((*src << 8) | 0x40000000);
			src++;
			buffer += 2;
		}
		buffer -= frame_adjust_1;
		for (c = 0; c < channels; ++c) {
			*buffer = cpu_to_be32((*src << 8) | 0x40000000);
			src++;
			buffer += 2;
		}
		buffer -= frame_adjust_2;
	}
}

static void amdtp_read_s32(struct amdtp_stream *s,
			   struct snd_pcm_substream *pcm,
			   __be32 *buffer, unsigned int frames)
{
	struct snd_pcm_runtime *runtime = pcm->runtime;
	unsigned int channels, remaining_frames, i, c;
	u32 *dst;

	channels = s->pcm_channels;
	dst  = (void *)runtime->dma_area +
			frames_to_bytes(runtime, s->pcm_buffer_pointer);
	remaining_frames = runtime->buffer_size - s->pcm_buffer_pointer;

	for (i = 0; i < frames; ++i) {
		for (c = 0; c < channels; ++c) {
			*dst = be32_to_cpu(buffer[c]) << 8;
			dst++;
		}
		buffer += s->data_block_quadlets;
		if (--remaining_frames == 0)
			dst = (void *)runtime->dma_area;
	}
}

static void amdtp_read_s32_dualwire(struct amdtp_stream *s,
				    struct snd_pcm_substream *pcm,
				    __be32 *buffer, unsigned int frames)
{
	struct snd_pcm_runtime *runtime = pcm->runtime;
	unsigned int channels, remaining_frames, i, c;
	u32 *dst;

	dst = (void *)runtime->dma_area +
			frames_to_bytes(runtime, s->pcm_buffer_pointer);
	remaining_frames = runtime->buffer_size - s->pcm_buffer_pointer;
	channels = s->pcm_channels / 2;

	for (i = 0; i < frames; ++i) {
		for (c = 0; c < channels; ++c) {
			*dst = be32_to_cpu(buffer[c * 2]) << 8;
			dst++;
		}
		buffer += 1;
		for (c = 0; c < channels; ++c) {
			*dst = be32_to_cpu(buffer[c * 2]) << 8;
			dst++;
		}
		buffer += s->data_block_quadlets - 1;
		if (--remaining_frames == 0)
			dst = (void *)runtime->dma_area;
	}
}

static void amdtp_fill_pcm_silence(struct amdtp_stream *s,
				   __be32 *buffer, unsigned int frames)
{
	unsigned int i, c;

	for (i = 0; i < frames; ++i) {
		for (c = 0; c < s->pcm_channels; ++c)
			buffer[c] = cpu_to_be32(0x40000000);
		buffer += s->data_block_quadlets;
	}
}

static void amdtp_fill_midi(struct amdtp_stream *s,
			    __be32 *buffer, unsigned int frames)
{
	unsigned int f, port;
	u8 *b;

	for (f = 0; f < frames; f++) {
		buffer[s->pcm_channels + 1] = 0;
		b = (u8 *)&buffer[s->pcm_channels + 1];

		port = (s->data_block_counter + f) % 8;
		if ((s->midi[port] == NULL) ||
		    (snd_rawmidi_transmit(s->midi[port], b + 1, 1) <= 0))
			b[0] = 0x80;
		else
			b[0] = 0x81;

		buffer += s->data_block_quadlets;
	}
}

static void amdtp_pull_midi(struct amdtp_stream *s,
			    __be32 *buffer, unsigned int frames)
{
	unsigned int f, port;
	int len;
	u8 *b;

	for (f = 0; f < frames; f++) {
		port = (s->data_block_counter + f) % 8;
		b = (u8 *)&buffer[s->pcm_channels + 1];

		len = b[0] - 0x80;
		if ((1 <= len) &&  (len <= 3) && (s->midi[port]))
			snd_rawmidi_receive(s->midi[port], b + 1, len);

		buffer += s->data_block_quadlets;
	}
}

static void update_pcm_pointers(struct amdtp_stream *s,
				struct snd_pcm_substream *pcm,
				unsigned int frames)
{	unsigned int ptr;

	if (s->dual_wire)
		frames *= 2;

	ptr = s->pcm_buffer_pointer + frames;
	if (ptr >= pcm->runtime->buffer_size)
		ptr -= pcm->runtime->buffer_size;
	ACCESS_ONCE(s->pcm_buffer_pointer) = ptr;

	s->pcm_period_pointer += frames;
	if (s->pcm_period_pointer >= pcm->runtime->period_size) {
		s->pcm_period_pointer -= pcm->runtime->period_size;
		s->pointer_flush = false;
		tasklet_hi_schedule(&s->period_tasklet);
	}
}

static void pcm_period_tasklet(unsigned long data)
{
	struct amdtp_stream *s = (void *)data;
	struct snd_pcm_substream *pcm = ACCESS_ONCE(s->pcm);

	if (pcm)
		snd_pcm_period_elapsed(pcm);
}

static int queue_packet(struct amdtp_stream *s,
			unsigned int header_length,
			unsigned int payload_length, bool skip)
{
	struct fw_iso_packet p = {0};
	int err = 0;

	if (IS_ERR(s->context))
		goto end;

	p.interrupt = IS_ALIGNED(s->packet_index + 1, INTERRUPT_INTERVAL);
	p.tag = TAG_CIP;
	p.header_length = header_length;
	p.payload_length = (!skip) ? payload_length : 0;
	p.skip = skip;
	err = fw_iso_context_queue(s->context, &p, &s->buffer.iso_buffer,
				   s->buffer.packets[s->packet_index].offset);
	if (err < 0) {
		dev_err(&s->unit->device, "queueing error: %d\n", err);
		goto end;
	}

	if (++s->packet_index >= QUEUE_LENGTH)
		s->packet_index = 0;
end:
	return err;
}

static inline int queue_out_packet(struct amdtp_stream *s,
				   unsigned int payload_length, bool skip)
{
	return queue_packet(s, OUT_PACKET_HEADER_SIZE,
			    payload_length, skip);
}

static inline int queue_in_packet(struct amdtp_stream *s)
{
	return queue_packet(s, IN_PACKET_HEADER_SIZE,
			    amdtp_stream_get_max_payload(s), false);
}

static void handle_out_packet(struct amdtp_stream *s, unsigned int syt)
{
	__be32 *buffer;
	unsigned int data_blocks, payload_length;
	struct snd_pcm_substream *pcm;

	if (s->packet_index < 0)
		return;

	/* this module generate empty packet for 'no data' */
	if (!(s->flags & CIP_BLOCKING) || (syt != CIP_SYT_NO_INFO))
		data_blocks = calculate_data_blocks(s);
	else
		data_blocks = 0;

	buffer = s->buffer.packets[s->packet_index].buffer;
	buffer[0] = cpu_to_be32(ACCESS_ONCE(s->source_node_id_field) |
				(s->data_block_quadlets << AMDTP_DBS_SHIFT) |
				s->data_block_counter);
	buffer[1] = cpu_to_be32(CIP_EOH | CIP_FMT_AM | AMDTP_FDF_AM824 |
				(s->sfc << CIP_FDF_SFC_SHIFT) | syt);
	buffer += 2;

	pcm = ACCESS_ONCE(s->pcm);
	if (pcm)
		s->transfer_samples(s, pcm, buffer, data_blocks);
	else
		amdtp_fill_pcm_silence(s, buffer, data_blocks);
	if (s->midi_ports)
		amdtp_fill_midi(s, buffer, data_blocks);

	s->data_block_counter = (s->data_block_counter + data_blocks) & 0xff;

	payload_length = 8 + data_blocks * 4 * s->data_block_quadlets;
	if (queue_out_packet(s, payload_length, false) < 0) {
		s->packet_index = -1;
		amdtp_stream_pcm_abort(s);
		return;
	}

	if (pcm)
		update_pcm_pointers(s, pcm, data_blocks);
}

static void handle_in_packet(struct amdtp_stream *s,
			     unsigned int payload_quadlets,
			     __be32 *buffer)
{
	u32 cip_header[2];
	unsigned int data_blocks, data_block_quadlets, data_block_counter;
	struct snd_pcm_substream *pcm = NULL;

	cip_header[0] = be32_to_cpu(buffer[0]);
	cip_header[1] = be32_to_cpu(buffer[1]);

	/*
	 * This module supports 'Two-quadlet CIP header with SYT field'.
	 * For convinience, also check FMT field is AM824 or not.
	 */
	if (((cip_header[0] & CIP_EOH_MASK) == CIP_EOH) ||
	    ((cip_header[1] & CIP_EOH_MASK) != CIP_EOH) ||
	    ((cip_header[1] & CIP_FMT_MASK) != CIP_FMT_AM)) {
		dev_info_ratelimited(&s->unit->device,
				"Invalid CIP header for AMDTP: %08X:%08X\n",
				cip_header[0], cip_header[1]);
		goto end;
	}

	/* Calculate data blocks */
	if (payload_quadlets < 3 ||
	    ((cip_header[1] & CIP_FDF_MASK) ==
				(AMDTP_FDF_NO_DATA << CIP_FDF_SFC_SHIFT))) {
		data_blocks = 0;
	} else {
		data_block_quadlets =
			(cip_header[0] & AMDTP_DBS_MASK) >> AMDTP_DBS_SHIFT;
		/* avoid division by zero */
		if (data_block_quadlets == 0) {
			dev_info_ratelimited(&s->unit->device,
				"Detect invalid value in dbs field: %08X\n",
				cip_header[0]);
			goto err;
		}

		data_blocks = (payload_quadlets - 2) / data_block_quadlets;
	}

	/* Check data block counter continuity */
	data_block_counter = cip_header[0] & AMDTP_DBC_MASK;
	if (data_block_counter != s->data_block_counter) {
		dev_info(&s->unit->device,
			 "Detect discontinuity of CIP: %02X %02X\n",
			 s->data_block_counter, data_block_counter);
		goto err;
	}

	if (data_blocks > 0) {
		buffer += 2;

		pcm = ACCESS_ONCE(s->pcm);
		if (pcm)
			s->transfer_samples(s, pcm, buffer, data_blocks);

		if (s->midi_ports)
			amdtp_pull_midi(s, buffer, data_blocks);
	}

	s->data_block_counter = (data_block_counter + data_blocks) & 0xff;
end:
	if (queue_in_packet(s) < 0)
		goto err;

	if (pcm)
		update_pcm_pointers(s, pcm, data_blocks);

	return;
err:
	s->packet_index = -1;
	amdtp_stream_pcm_abort(s);
}

static void out_stream_callback(struct fw_iso_context *context, u32 cycle,
				size_t header_length, void *header,
				void *private_data)
{
	struct amdtp_stream *s = private_data;
	unsigned int i, syt, packets = header_length / 4;

	/*
	 * Compute the cycle of the last queued packet.
	 * (We need only the four lowest bits for the SYT, so we can ignore
	 * that bits 0-11 must wrap around at 3072.)
	 */
	cycle += QUEUE_LENGTH - packets;

	for (i = 0; i < packets; ++i) {
		syt = calculate_syt(s, ++cycle);
		handle_out_packet(s, syt);
	}
	fw_iso_context_queue_flush(s->context);
}

static void in_stream_callback(struct fw_iso_context *context, u32 cycle,
			       size_t header_length, void *header,
			       void *private_data)
{
	struct amdtp_stream *s = private_data;
	unsigned int p, syt, packets, payload_quadlets;
	__be32 *buffer, *headers = header;

	/* The number of packets in buffer */
	packets = header_length / IN_PACKET_HEADER_SIZE;

	for (p = 0; p < packets; p++) {
		if (s->packet_index < 0)
			break;

		buffer = s->buffer.packets[s->packet_index].buffer;

		/* Process sync slave stream */
		if (s->sync_slave && s->sync_slave->callbacked) {
			syt = be32_to_cpu(buffer[1]) & CIP_SYT_MASK;
			handle_out_packet(s->sync_slave, syt);
		}

		/* The number of quadlets in this packet */
		payload_quadlets =
			(be32_to_cpu(headers[p]) >> ISO_DATA_LENGTH_SHIFT) / 4;
		handle_in_packet(s, payload_quadlets, buffer);
	}

	/* Queueing error or detecting discontinuity */
	if (s->packet_index < 0) {
		/* Abort sync slave. */
		if (s->sync_slave) {
			s->sync_slave->packet_index = -1;
			amdtp_stream_pcm_abort(s->sync_slave);
		}
		return;
	}

	/* when sync to device, flush the packets for slave stream */
	if (s->sync_slave && s->sync_slave->callbacked)
		fw_iso_context_queue_flush(s->sync_slave->context);

	fw_iso_context_queue_flush(s->context);
}

/* processing is done by master callback */
static void slave_stream_callback(struct fw_iso_context *context, u32 cycle,
				  size_t header_length, void *header,
				  void *private_data)
{
	return;
}

/* this is executed one time */
static void amdtp_stream_first_callback(struct fw_iso_context *context,
					u32 cycle, size_t header_length,
					void *header, void *private_data)
{
	struct amdtp_stream *s = private_data;

	/*
	 * For in-stream, first packet has come.
	 * For out-stream, prepared to transmit first packet
	 */
	s->callbacked = true;
	wake_up(&s->callback_wait);

	if (s->direction == AMDTP_IN_STREAM)
		context->callback.sc = in_stream_callback;
	else if ((s->flags & CIP_BLOCKING) && (s->flags & CIP_SYNC_TO_DEVICE))
		context->callback.sc = slave_stream_callback;
	else
		context->callback.sc = out_stream_callback;

	context->callback.sc(context, cycle, header_length, header, s);
}

/**
 * amdtp_stream_start - start transferring packets
 * @s: the AMDTP stream to start
 * @channel: the isochronous channel on the bus
 * @speed: firewire speed code
 *
 * The stream cannot be started until it has been configured with
 * amdtp_stream_set_parameters() and it must be started before any PCM or MIDI
 * device can be started.
 */
int amdtp_stream_start(struct amdtp_stream *s, int channel, int speed)
{
	static const struct {
		unsigned int data_block;
		unsigned int syt_offset;
	} initial_state[] = {
		[CIP_SFC_32000]  = {  4, 3072 },
		[CIP_SFC_48000]  = {  6, 1024 },
		[CIP_SFC_96000]  = { 12, 1024 },
		[CIP_SFC_192000] = { 24, 1024 },
		[CIP_SFC_44100]  = {  0,   67 },
		[CIP_SFC_88200]  = {  0,   67 },
		[CIP_SFC_176400] = {  0,   67 },
	};
	unsigned int header_size;
	enum dma_data_direction dir;
	int type, err;

	mutex_lock(&s->mutex);

	if (WARN_ON(amdtp_stream_running(s) ||
		    (s->data_block_quadlets < 1))) {
		err = -EBADFD;
		goto err_unlock;
	}

	s->data_block_counter = 0;
	s->data_block_state = initial_state[s->sfc].data_block;
	s->syt_offset_state = initial_state[s->sfc].syt_offset;
	s->last_syt_offset = TICKS_PER_CYCLE;

	/* initialize packet buffer */
	if (s->direction == AMDTP_IN_STREAM) {
		dir = DMA_FROM_DEVICE;
		type = FW_ISO_CONTEXT_RECEIVE;
		header_size = IN_PACKET_HEADER_SIZE;
	} else {
		dir = DMA_TO_DEVICE;
		type = FW_ISO_CONTEXT_TRANSMIT;
		header_size = OUT_PACKET_HEADER_SIZE;
	}
	err = iso_packets_buffer_init(&s->buffer, s->unit, QUEUE_LENGTH,
				      amdtp_stream_get_max_payload(s), dir);
	if (err < 0)
		goto err_unlock;

	s->context = fw_iso_context_create(fw_parent_device(s->unit)->card,
					   type, channel, speed, header_size,
					   amdtp_stream_first_callback, s);
	if (IS_ERR(s->context)) {
		err = PTR_ERR(s->context);
		if (err == -EBUSY)
			dev_err(&s->unit->device,
				"no free stream on this controller\n");
		goto err_buffer;
	}

	amdtp_stream_update(s);

	s->packet_index = 0;
	do {
		if (s->direction == AMDTP_IN_STREAM)
			err = queue_in_packet(s);
		else
			err = queue_out_packet(s, 0, true);
		if (err < 0)
			goto err_context;
	} while (s->packet_index > 0);

	/* NOTE: TAG1 matches CIP. This just affects in stream. */
	s->callbacked = false;
	err = fw_iso_context_start(s->context, -1, 0,
				   FW_ISO_CONTEXT_MATCH_TAG1);
	if (err < 0)
		goto err_context;

	mutex_unlock(&s->mutex);

	return 0;

err_context:
	fw_iso_context_destroy(s->context);
	s->context = ERR_PTR(-1);
err_buffer:
	iso_packets_buffer_destroy(&s->buffer, s->unit);
err_unlock:
	mutex_unlock(&s->mutex);

	return err;
}
EXPORT_SYMBOL(amdtp_stream_start);

/**
 * amdtp_stream_pcm_pointer - get the PCM buffer position
 * @s: the AMDTP stream that transports the PCM data
 *
 * Returns the current buffer position, in frames.
 */
unsigned long amdtp_stream_pcm_pointer(struct amdtp_stream *s)
{
	/* this optimization is allowed to be racy */
	if (s->pointer_flush)
		fw_iso_context_flush_completions(s->context);
	else
		s->pointer_flush = true;

	return ACCESS_ONCE(s->pcm_buffer_pointer);
}
EXPORT_SYMBOL(amdtp_stream_pcm_pointer);

/**
 * amdtp_stream_update - update the stream after a bus reset
 * @s: the AMDTP stream
 */
void amdtp_stream_update(struct amdtp_stream *s)
{
	ACCESS_ONCE(s->source_node_id_field) =
		(fw_parent_device(s->unit)->card->node_id & 0x3f) << 24;
}
EXPORT_SYMBOL(amdtp_stream_update);

/**
 * amdtp_stream_stop - stop sending packets
 * @s: the AMDTP stream to stop
 *
 * All PCM and MIDI devices of the stream must be stopped before the stream
 * itself can be stopped.
 */
void amdtp_stream_stop(struct amdtp_stream *s)
{
	mutex_lock(&s->mutex);

	if (!amdtp_stream_running(s)) {
		mutex_unlock(&s->mutex);
		return;
	}

	tasklet_kill(&s->period_tasklet);
	fw_iso_context_stop(s->context);
	fw_iso_context_destroy(s->context);
	s->context = ERR_PTR(-1);
	iso_packets_buffer_destroy(&s->buffer, s->unit);

	s->callbacked = false;

	mutex_unlock(&s->mutex);
}
EXPORT_SYMBOL(amdtp_stream_stop);

/**
 * amdtp_stream_pcm_abort - abort the running PCM device
 * @s: the AMDTP stream about to be stopped
 *
 * If the isochronous stream needs to be stopped asynchronously, call this
 * function first to stop the PCM device.
 */
void amdtp_stream_pcm_abort(struct amdtp_stream *s)
{
	struct snd_pcm_substream *pcm;

	pcm = ACCESS_ONCE(s->pcm);
	if (pcm) {
		snd_pcm_stream_lock_irq(pcm);
		if (snd_pcm_running(pcm))
			snd_pcm_stop(pcm, SNDRV_PCM_STATE_XRUN);
		snd_pcm_stream_unlock_irq(pcm);
	}
}
EXPORT_SYMBOL(amdtp_stream_pcm_abort);
