/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2020 Winlin
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef SRS_KERNEL_RTP_HPP
#define SRS_KERNEL_RTP_HPP

#include <srs_core.hpp>

#include <srs_kernel_buffer.hpp>
#include <srs_kernel_codec.hpp>

#include <string>

class SrsRtpPacket2;

const int kRtpHeaderFixedSize = 12;
const uint8_t kRtpMarker = 0x80;

// H.264 nalu header type mask.
const uint8_t kNalTypeMask      = 0x1F;

// @see: https://tools.ietf.org/html/rfc6184#section-5.2
const uint8_t kStapA            = 24;

// @see: https://tools.ietf.org/html/rfc6184#section-5.2
const uint8_t kFuA              = 28;

// @see: https://tools.ietf.org/html/rfc6184#section-5.8
const uint8_t kStart            = 0x80; // Fu-header start bit
const uint8_t kEnd              = 0x40; // Fu-header end bit


class SrsBuffer;
class SrsRtpRawPayload;
class SrsRtpFUAPayload2;

class SrsRtpHeader
{
private:
    bool padding;
    uint8_t padding_length;
    bool extension;
    uint8_t cc;
    bool marker;
    uint8_t payload_type;
    uint16_t sequence;
    int32_t timestamp;
    uint32_t ssrc;
    uint32_t csrc[15];
    uint16_t extension_length;
    // TODO:extension field.
public:
    SrsRtpHeader();
    virtual ~SrsRtpHeader();
    void reset();
public:
    virtual srs_error_t decode(SrsBuffer* buf);
    virtual srs_error_t encode(SrsBuffer* buf);
    virtual int nb_bytes();
public:
    void set_marker(bool v);
    bool get_marker() const;
    void set_payload_type(uint8_t v);
    uint8_t get_payload_type() const;
    void set_sequence(uint16_t v);
    uint16_t get_sequence() const;
    void set_timestamp(int64_t v);
    int64_t get_timestamp() const;
    void set_ssrc(uint32_t v);
    uint32_t get_ssrc() const;
    void set_padding(bool v);
    void set_padding_length(uint8_t v);
    uint8_t get_padding_length() const;
};

class ISrsRtpPacketDecodeHandler
{
public:
    ISrsRtpPacketDecodeHandler();
    virtual ~ISrsRtpPacketDecodeHandler();
public:
    // We don't know the actual payload, so we depends on external handler.
    virtual void on_before_decode_payload(SrsRtpPacket2* pkt, SrsBuffer* buf, ISrsCodec** ppayload) = 0;
};

class SrsRtpPacket2
{
// RTP packet fields.
public:
    // TODO: FIXME: Rename to header.
    SrsRtpHeader rtp_header;
    ISrsCodec* payload;
    // TODO: FIXME: Merge into rtp_header.
    int padding;
// Decoder helper.
public:
    // Helper information for decoder.
    bool is_first_packet_of_frame;
    bool is_last_packet_of_frame;
    bool is_key_frame;
    // The first byte as nalu type, for video decoder only.
    SrsAvcNaluType nalu_type;
    // The original payload bytes length.
    int nn_original_payload;
// Decoder helper.
private:
    // The original bytes for decoder only, we will free it.
    char* original_bytes;
    int nn_original_bytes;
// Fast cache for performance.
private:
    // Cache frequently used payload for performance.
    SrsRtpRawPayload* cache_raw;
    SrsRtpFUAPayload2* cache_fua;
    int cache_payload;
    // The helper handler for decoder, use RAW payload if NULL.
    ISrsRtpPacketDecodeHandler* decode_handler;
public:
    SrsRtpPacket2();
    virtual ~SrsRtpPacket2();
public:
    // Set the padding of RTP packet.
    void set_padding(int size);
    // Increase the padding of RTP packet.
    void add_padding(int size);
    // Reset RTP packet.
    void reset();
    // Reuse the cached raw message as payload.
    SrsRtpRawPayload* reuse_raw();
    // Reuse the cached fua message as payload.
    SrsRtpFUAPayload2* reuse_fua();
    // Set the decode handler.
    void set_decode_handler(ISrsRtpPacketDecodeHandler* h);
    // Set the original bytes.
    void set_original_bytes(char* buf, int nn_buf);
// interface ISrsEncoder
public:
    virtual int nb_bytes();
    virtual srs_error_t encode(SrsBuffer* buf);
    virtual srs_error_t decode(SrsBuffer* buf);
};

// Single payload data.
class SrsRtpRawPayload : public ISrsCodec
{
public:
    // The RAW payload, directly point to the shared memory.
    // @remark We only refer to the memory, user must free its bytes.
    char* payload;
    int nn_payload;
public:
    SrsRtpRawPayload();
    virtual ~SrsRtpRawPayload();
// interface ISrsEncoder
public:
    virtual int nb_bytes();
    virtual srs_error_t encode(SrsBuffer* buf);
    virtual srs_error_t decode(SrsBuffer* buf);
};

// Multiple NALUs, automatically insert 001 between NALUs.
class SrsRtpRawNALUs : public ISrsCodec
{
private:
    // We will manage the samples, but the sample itself point to the shared memory.
    std::vector<SrsSample*> nalus;
    int nn_bytes;
    int cursor;
public:
    SrsRtpRawNALUs();
    virtual ~SrsRtpRawNALUs();
public:
    void push_back(SrsSample* sample);
public:
    uint8_t skip_first_byte();
    // We will manage the returned samples, if user want to manage it, please copy it.
    srs_error_t read_samples(std::vector<SrsSample*>& samples, int packet_size);
// interface ISrsEncoder
public:
    virtual int nb_bytes();
    virtual srs_error_t encode(SrsBuffer* buf);
    virtual srs_error_t decode(SrsBuffer* buf);
};

// STAP-A, for multiple NALUs.
class SrsRtpSTAPPayload : public ISrsCodec
{
public:
    // The NRI in NALU type.
    SrsAvcNaluType nri;
    // The NALU samples, we will manage the samples.
    // @remark We only refer to the memory, user must free its bytes.
    std::vector<SrsSample*> nalus;
public:
    SrsRtpSTAPPayload();
    virtual ~SrsRtpSTAPPayload();
// interface ISrsEncoder
public:
    virtual int nb_bytes();
    virtual srs_error_t encode(SrsBuffer* buf);
    virtual srs_error_t decode(SrsBuffer* buf);
};

// FU-A, for one NALU with multiple fragments.
// With more than one payload.
class SrsRtpFUAPayload : public ISrsCodec
{
public:
    // The NRI in NALU type.
    SrsAvcNaluType nri;
    // The FUA header.
    bool start;
    bool end;
    SrsAvcNaluType nalu_type;
    // The NALU samples, we manage the samples.
    // @remark We only refer to the memory, user must free its bytes.
    std::vector<SrsSample*> nalus;
public:
    SrsRtpFUAPayload();
    virtual ~SrsRtpFUAPayload();
// interface ISrsEncoder
public:
    virtual int nb_bytes();
    virtual srs_error_t encode(SrsBuffer* buf);
    virtual srs_error_t decode(SrsBuffer* buf);
};

// FU-A, for one NALU with multiple fragments.
// With only one payload.
class SrsRtpFUAPayload2 : public ISrsCodec
{
public:
    // The NRI in NALU type.
    SrsAvcNaluType nri;
    // The FUA header.
    bool start;
    bool end;
    SrsAvcNaluType nalu_type;
    // The payload and size,
    char* payload;
    int size;
public:
    SrsRtpFUAPayload2();
    virtual ~SrsRtpFUAPayload2();
// interface ISrsEncoder
public:
    virtual int nb_bytes();
    virtual srs_error_t encode(SrsBuffer* buf);
    virtual srs_error_t decode(SrsBuffer* buf);
};

class SrsRtpPayloadHeader
{
public:
    bool is_first_packet_of_frame;
    bool is_last_packet_of_frame;
    bool is_key_frame;
public:
    SrsRtpPayloadHeader();
    virtual ~SrsRtpPayloadHeader();
    SrsRtpPayloadHeader(const SrsRtpPayloadHeader& rhs);
    SrsRtpPayloadHeader& operator=(const SrsRtpPayloadHeader& rhs);
};

class SrsRtpOpusHeader : public SrsRtpPayloadHeader
{
public:
    SrsRtpOpusHeader();
    virtual ~SrsRtpOpusHeader();
    SrsRtpOpusHeader(const SrsRtpOpusHeader& rhs);
    SrsRtpOpusHeader& operator=(const SrsRtpOpusHeader& rhs);
};

class SrsRtpH264Header : public SrsRtpPayloadHeader
{
public:
    uint8_t nalu_type;
    uint8_t nalu_header;
    std::vector<std::pair<size_t, size_t> > nalu_offset; // offset, size
public:
    SrsRtpH264Header();
    virtual ~SrsRtpH264Header();
    SrsRtpH264Header(const SrsRtpH264Header& rhs);
    SrsRtpH264Header& operator=(const SrsRtpH264Header& rhs);
};

// TODO: FIXME: Merge it with shared message.
class SrsRtpSharedPacket
{
private:
    class SrsRtpSharedPacketPayload 
    {   
    public:
        // Rtp packet buffer, include rtp header and payload.
        char* payload;
        int size;
        int shared_count;
    public:
        SrsRtpSharedPacketPayload();
        virtual ~SrsRtpSharedPacketPayload();
    };  
private:
    SrsRtpSharedPacketPayload* payload_ptr;
public:
    SrsRtpHeader rtp_header;
    SrsRtpPayloadHeader* rtp_payload_header;
    char* payload;
    int size;
public:
    SrsRtpSharedPacket();
    virtual ~SrsRtpSharedPacket();
public:
    srs_error_t create(SrsRtpHeader* rtp_h, SrsRtpPayloadHeader* rtp_ph, char* p, int s);
    srs_error_t decode(char* buf, int nb_buf);
    SrsRtpSharedPacket* copy();
public:
    char* rtp_payload() { return payload + rtp_header.nb_bytes(); }
    int rtp_payload_size() { return size - rtp_header.nb_bytes() - rtp_header.get_padding_length(); }
// Interface to modify rtp header 
public:
    srs_error_t modify_rtp_header_marker(bool marker);
    srs_error_t modify_rtp_header_ssrc(uint32_t ssrc);
    srs_error_t modify_rtp_header_payload_type(uint8_t payload_type);
};

#endif
