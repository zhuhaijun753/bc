#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include "../inc/dep.h"
#include "../inc/bcp_packet.h"
#include "../inc/crc32.h"
#include "../inc/util/Thread.h"

static u64 seq_id = 0;
static mutex_type mutex = NULL;

void bcp_packet_init(void)
{
	mutex = Thread_create_mutex();
}

void bcp_packet_uninit(void)
{
	if (mutex) {
		Thread_destroy_mutex(mutex);
		mutex = NULL;
	}
}

u64 bcp_next_seq_id(void)
{
	u64 id = 0;

	Thread_lock_mutex(mutex);
	id = seq_id++;
	Thread_unlock_mutex(mutex);

	return id;
}

bcp_packet_t *bcp_packet_create(u8 version)
{
	bcp_packet_t *p;
	
	p = (bcp_packet_t*)malloc(sizeof(*p));
	if (!p) {
		return NULL;
	}

	memset(p, 0, sizeof(*p));

	p->hdr.sof[0] = 0x3f;
	p->hdr.sof[1] = 0x6c;
	p->hdr.sof[2] = 0x81;
	p->hdr.sof[3] = 0x29;
	p->hdr.version = version;

	ListZero(&p->messages);

	p->end.crc32 = 0;
	p->end.eof[0] = 0x68;
	p->end.eof[1] = 0x1a;
	p->end.eof[2] = 0x5b;
	p->end.eof[3] = 0x90;

	return p;
}

void bcp_element_destroy(bcp_element_t *e);
void bcp_message_destroy(bcp_message_t *m);

void bcp_elements_destroy(List *list)
{
	bcp_element_t *e;
	ListElement* current = NULL;

	while (ListNextElement(list, &current) != NULL) {
		e = (bcp_element_t*)current->content;
		if (e) {
			bcp_element_destroy(e);
			current->content = NULL;
		}
	}

	ListEmpty(list);
}

void bcp_messages_destroy(List *list)
{
	bcp_message_t *m;
	ListElement* current = NULL;

	while (ListNextElement(list, &current) != NULL) {
		m = (bcp_message_t*)current->content;
		if (m) {
			bcp_message_destroy(m);
			current->content = NULL;
		}
	}

	ListEmpty(list);
}

void bcp_packet_destroy(bcp_packet_t *p)
{
	if (!p) {
		return;
	}

	bcp_messages_destroy(&p->messages);
	free(p);
}

static u32 element_hdr_size(bcp_element_t *e)
{
	return 2;
}

static u32 element_size(bcp_element_t *e)
{
	if (e) {
		return e->len + element_hdr_size(e);
	} else {
		return 0;
	}
}

static u32 message_hdr_size(bcp_message_t *m)
{
	return 15;
}

static u32 datagram_hdr_size(bcp_packet_t *p)
{
	return 8;
}

static u32 datagram_sof_size(bcp_packet_t *p)
{
	return 4;
}

static u32 datagram_eof_size(bcp_packet_t *p)
{
	return 4;
}

static u32 datagram_crc_size(bcp_packet_t *p)
{
	return datagram_hdr_size(p) - datagram_sof_size(p);
}

static u32 datagram_end_size(bcp_packet_t *p)
{
	return 8;
}

bcp_message_t *bcp_message_create(u16 application_id, 
	u8 step_id, u8 version, u8 session_id)
{
	bcp_message_t *m;

	m = (bcp_message_t*)malloc(sizeof(*m));
	if (!m) {
		return NULL;
	}

	memset(m, 0, sizeof(*m));

	/* application hdr */
	m->hdr.id = application_id;
	m->hdr.step_id = step_id;
	m->hdr.session_id = session_id;
	m->hdr.version = version;
	m->hdr.sequence_id = bcp_next_seq_id();
	m->hdr.message_len = 0;

	return m;
}

void bcp_message_append(bcp_packet_t *p, bcp_message_t *m)
{
	if (!p || !m) {
		return;
	}

	if (!m->hdr.sequence_id) {
		m->hdr.sequence_id = bcp_next_seq_id();
	}

	ListAppend(&p->messages, m, sizeof(*m));
	p->hdr.packet_len += message_hdr_size(m);
	m->p = p;
}

void bcp_messages_foreach(bcp_packet_t *p, bcp_message_foreach_callback_t *cb, void *context)
{
	bcp_message_t *m;
	ListElement* current = NULL;

	while (ListNextElement(&p->messages, &current) != NULL) {
		m = (bcp_message_t*)current->content;
		if (m) {
			cb(m, context);
		}
	}
}

void bcp_message_destroy(bcp_message_t *m)
{
	if (!m) {
		return;
	}
	bcp_elements_destroy(&m->elements);
	if (m->p) {
		m->p->hdr.packet_len -= message_hdr_size(m);
	}
	free(m);
}

bcp_element_t *bcp_element_create(u8 *data, u32 len)
{
	bcp_element_t *e;
	u8 *pload;

	e = (bcp_element_t*)malloc(sizeof(*e));
	if (!e) {
		return NULL;
	}

	pload = (u8*)malloc(len);
	if (!pload) {
		free(e);
		return NULL;
	}

	memcpy(pload, data, len);

	e->len = len;
	e->data = pload;

	return e;
}

void bcp_element_append(bcp_message_t *m, bcp_element_t *e)
{
	u32 sz;

	if (!m || !e) {
		return;
	}

	ListAppend(&m->elements, e, sizeof(*e));

	sz = element_size(e);
	m->hdr.message_len += sz;
	if (m->p) {
		m->p->hdr.packet_len += sz;
	} else {
		LOG_W("message muste add to packet first.");
	}
	e->m = m;
}

void bcp_elements_foreach(bcp_message_t *m, bcp_element_foreach_callback_t *cb, void *context)
{
	bcp_element_t *e;
	ListElement* current = NULL;

	while (ListNextElement(&m->elements, &current) != NULL) {
		e = (bcp_element_t*)current->content;
		if (e) {
			cb(e, context);
		}
	}
}

void bcp_element_destroy(bcp_element_t *e)
{
	if (!e) {
		return;
	}
	if (e->len > 0 && e->data) {
		free(e->data);
	}
	if (e->m) {
		e->m->hdr.message_len -= element_size(e);
		if (e->m->p) {
			e->m->p->hdr.packet_len -= message_hdr_size(e->m);
		}
	}
	free(e);
}

#define DEF_CRC32 0xaabbccdd
u32 bcp_crc32(bcp_packet_t *p, u8 *buf, u32 len)
{
	u32 hdrlen, buflen;
	u8 *start;

	if (!p || !buf) {
		return DEF_CRC32;
	}

	hdrlen = datagram_crc_size(p);
	start = buf + datagram_sof_size(p); /* step sof */
	buflen = hdrlen + p->hdr.packet_len;
	return calc_crc32(start, buflen);
}

static u32 datagram_hdr_serialize(bcp_packet_t *p, u8 *buf, u32 i)
{
	/* sof */
	buf[i++] = p->hdr.sof[0];
	buf[i++] = p->hdr.sof[1];
	buf[i++] = p->hdr.sof[2];
	buf[i++] = p->hdr.sof[3];
	/* version */
	buf[i++] = p->hdr.version & 0xf;
	/* remaing length */
	buf[i++] = ((p->hdr.packet_len) >> 16) & 0xff;
	buf[i++] = ((p->hdr.packet_len) >> 8) & 0xff;
	buf[i++] = ((p->hdr.packet_len) >> 0) & 0xff;

	return i;
}

static u32 message_hdr_serialize(bcp_application_header_t *hdr, u8 *buf, u32 i)
{
	/* application id */
	buf[i++] = (hdr->id >> 8) & 0xff;
	buf[i++] = (hdr->id >> 0) & 0xff;
	buf[i++] = ((hdr->step_id & 0xf) << 4) | (hdr->version & 0xf);
	buf[i++] = (hdr->session_id) & 0xff;

	/* seq id */
	memcpy(&buf[i], &hdr->sequence_id, 8);
	i += 8;

	/* remaing length */
	buf[i++] = (hdr->message_len >> 16) & 0xff;
	buf[i++] = (hdr->message_len >> 8) & 0xff;
	buf[i++] = (hdr->message_len >> 0) & 0xff;
	
	return i;
}

static u32 element_serialize(bcp_element_t *e, u8 *buf, u32 i)
{
	if (e && e->data && e->len > 0) {
		buf[i++] = (e->len >> 8) & 0xff;
		buf[i++] = (e->len >> 0) & 0xff;
		memcpy(&buf[i], e->data, e->len);
		i += e->len;
	}
	return i;
}

static u32 elements_serialize(bcp_message_t *m, u8 *buf, u32 i)
{
	bcp_element_t *e;
	ListElement* current = NULL;

	while (ListNextElement(&m->elements, &current) != NULL) {
		e = (bcp_element_t*)current->content;
		if (e) {
			i = element_serialize(e, buf, i);
		}
	}

	return i;
}

static u32 messages_serialize(bcp_packet_t *p, u8 *buf, u32 i)
{
	bcp_message_t *m;
	ListElement* current = NULL;

	while (ListNextElement(&p->messages, &current) != NULL) {
		m = (bcp_message_t*)current->content;
		if (m) {
			i = message_hdr_serialize(&m->hdr, buf, i);
			i = elements_serialize(m, buf, i);
		}
	}

	return i;
}

static u32 datagram_end_serialize(bcp_packet_t *p, u8 *buf, u32 i)
{
	p->end.crc32 = bcp_crc32(p, buf, i);

	/* crc32 */
	buf[i++] = ((p->end.crc32) >> 24) & 0xff;
	buf[i++] = ((p->end.crc32) >> 16) & 0xff;
	buf[i++] = ((p->end.crc32) >> 8) & 0xff;
	buf[i++] = ((p->end.crc32) >> 0) & 0xff;
	/* eof */
	buf[i++] = p->end.eof[0];
	buf[i++] = p->end.eof[1];
	buf[i++] = p->hdr.sof[2];
	buf[i++] = p->hdr.sof[3];

	return i;
}

static u32 bcp_serialize_size(bcp_packet_t *p)
{
	if (!p) {
		return 0;
	}
	return p->hdr.packet_len + datagram_hdr_size(p) + datagram_end_size(p);
}

s32 bcp_packet_serialize(bcp_packet_t *p, u8 **buf, u32 *len)
{
	u8 *ibuf = NULL;
	u32 i, bytes;

	if (!p || !buf) {
		return -1;
	}

	if (!p->hdr.packet_len) {
		return -1;
	}

	bytes = bcp_serialize_size(p);

	ibuf = (u8*)malloc(bytes);
	if (!ibuf) {
		return -1;
	}

	i = 0;
	i = datagram_hdr_serialize(p, ibuf, i);
	i = messages_serialize(p, ibuf, i);
	i = datagram_end_serialize(p, ibuf, i);

	if (i != bytes) {
		free(ibuf);
		LOG_W("bcp_packet_serialize failed\n");
		return -1;
	}

	*buf = ibuf;
	*len = i;

	return 0;
}

static u32 datagram_hdr_unserialize(bcp_packet_t *p, 
	u8 *buf, u32 i, u32 len)
{
	u32 plen;

	if (i + datagram_hdr_size(p) > len) {
		return i;
	}

	p->hdr.sof[0] = buf[i++];
	p->hdr.sof[1] = buf[i++];
	p->hdr.sof[2] = buf[i++];
	p->hdr.sof[3] = buf[i++];
	p->hdr.version = buf[i++] & 0xf;

	plen = buf[i++] << 16;
	plen |= buf[i++] << 8;
	plen |= buf[i++] << 0;

	p->hdr.packet_len = plen;

	return i;
}

static u32 message_hdr_unserialize(bcp_message_t **m, 
	u8 *buf, u32 i, u32 len)
{
	bcp_message_t *nm = NULL;
	bcp_application_header_t hdr = {0,};

	*m = NULL;

	if (i + message_hdr_size(NULL) > len) {
		return i;
	}

	/* application id */
	hdr.id = buf[i++] << 8;
	hdr.id |= buf[i++] << 0;

	hdr.step_id = (buf[i] >> 4) & 0xf;
	hdr.version = (buf[i++] >> 0) & 0xf;
	hdr.session_id = (buf[i++]) & 0xff;

	/* seq id */
	memcpy(&hdr.sequence_id, &buf[i], 8);
	i += 8;

	/* remaining length */
	hdr.message_len = buf[i++] << 16;
	hdr.message_len |= buf[i++] << 8;
	hdr.message_len |= buf[i++] << 0;

	nm = bcp_message_create(hdr.id, hdr.step_id,
		hdr.version, hdr.session_id);
	if (!nm) {
		return i;
	}

	nm->hdr.message_len = hdr.message_len;
	*m = nm;
	
	return i;
}

static u32 element_unserialize(bcp_message_t *m, 
	u8 *buf, u32 i, u32 len, s32 *ret)
{
	bcp_element_t *e;
	u8 *data;
	u32 hdr_size, data_len;

	hdr_size = element_hdr_size(NULL);

	/* element header */
	if (i + hdr_size > len) {
		*ret = -1;
		return i;
	}

	data_len = buf[i++] << 8;
	data_len |= buf[i++] << 0;

	/* element total len */
	if (i + hdr_size + data_len > len) {
		*ret = -1;
		return i;
	}

	data = (u8*)malloc(data_len);
	if (!data) {
		*ret = -1;
		return i;
	}

	memcpy(data, &buf[i], data_len);
	i += data_len;

	e = bcp_element_create(data, data_len);
	if (!e) {
		free(data);
		*ret = -1;
		return i;
	}

	bcp_element_append(m, e);
	*ret = 0;

	return i;
}

static u32 message_unserialize(bcp_packet_t *p, 
	u8 *buf, u32 i, u32 len, s32 *ret)
{
	u32 end;
	u32 message_len;
	bcp_message_t *m = NULL;

	*ret = 0;

	i = message_hdr_unserialize(&m, buf, i, len);
	if (!m) {
		*ret = -1;
		return i;
	}

	bcp_message_append(p, m);

	message_len = m->hdr.message_len;
	end = i + message_len;
	m->hdr.message_len = 0; /* rebuild by element append */

	while (i < end && *ret >= 0) {
		i = element_unserialize(m, buf, i, len, ret);
	}

	if (message_len != m->hdr.message_len) {
		LOG_W("message_unserialize message len failed\n");
		bcp_message_destroy(m);
		*ret = -1;
		return i;
	}

	*ret = 0;

	return i;
}

static u32 messages_unserialize(bcp_packet_t *p, 
	u8 *buf, u32 i, u32 len)
{
	s32 ret;
	u32 end, packet_len;

	packet_len = p->hdr.packet_len;
	
	if (i + packet_len > len) {
		return i;
	}

	ret = 0;
	p->hdr.packet_len = 0; /* rebuild by message append */

	end = i + packet_len;
	while (i < end && ret >= 0 ) {
		i = message_unserialize(p, buf, i, len, &ret);
	}

	if (packet_len != p->hdr.packet_len) {
		LOG_W("messages_unserialize packet len failed\n");
		bcp_messages_destroy(&p->messages);
		return i;
	}

	return i;
}

static u32 datagram_end_unserialize(bcp_packet_t *p, u8 *buf, u32 i, u32 len)
{
	u32 crc32 = 0;

	if (i + datagram_end_size(p) > len) {
		return i;
	}

	crc32 = buf[i++] << 24;
	crc32 |= buf[i++] << 16;
	crc32 |= buf[i++] << 8;
	crc32 |= buf[i++] << 0;

	p->end.crc32 = crc32;
	
	p->end.eof[0] = buf[i++];
	p->end.eof[1] = buf[i++];
	p->end.eof[2] = buf[i++];
	p->end.eof[3] = buf[i++];

	return i;
}

s32 bcp_packet_unserialize(u8 *buf, u32 len, bcp_packet_t **p)
{
	u32 i, crc32;
	bcp_packet_t *pk;
	u32 sof_size, eof_size;
	
	if (!buf || !p) {
		return -1;
	}

	pk = bcp_packet_create(0);
	if (!pk) {
		return -1;
	}

	sof_size = datagram_hdr_size(pk);
	eof_size = datagram_end_size(pk);

	if (len < sof_size + eof_size) {
		bcp_packet_destroy(pk);
		LOG_W("bcp_packet_unserialize buf size failed.\n");
		return -1;
	}

	/* validation sof & eof */
	if (!memcmp(&buf[0], &pk->hdr.sof[0], sof_size) ||
		!memcmp(&buf[len - eof_size], &pk->end.eof[0], eof_size)) {
		bcp_packet_destroy(pk);
		LOG_W("bcp_packet_unserialize sof or eof failed.\n");
		return -2;
	}

	i = 0;
	i = datagram_hdr_unserialize(pk, buf, i, len);
	i = messages_unserialize(pk, buf, i, len);
	i = datagram_end_unserialize(pk, buf, i, len);

	if (i != bcp_serialize_size(pk)) {
		bcp_packet_destroy(pk);
		LOG_W("bcp_packet_unserialize decode failed.\n");
		return -3;
	}

	crc32 = bcp_crc32(pk, buf, len);
	if (crc32 != pk->end.crc32) {
		bcp_packet_destroy(pk);
		LOG_W("bcp_packet_unserialize crc32 failed.\n");
		return -4;
	}

	*p = pk;

	return 0;
}