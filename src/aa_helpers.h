//
// Created by Besogonov, Aleksei on 1/26/16.
//

#ifndef AAUTO_AA_HELPER_H
#define AAUTO_AA_HELPER_H

#include "utils.h"

// The minimum size of the AA packet: channel + flags + 2 byte len + 2 byte msg type
static const int AA_PACKET_HEADER_SIZE = 6;

// The maximum AA packet (MAX_SHORT + 2 bytes len + 1 byte flags + 1 byte channel)
// and some space just in case
static const int MAX_AA_PACKET = 65536+8;

struct packet_t
{
    u_char chan_;
    bool encrypted_, control_;
    buf_t content_;
};
typedef std::shared_ptr<packet_t> packet_ptr_t;

enum aa_message_types {
    AA_RE
};

enum aa_channels {
    AA_CONTROL_CHANNEL = 0,
    AA_SENSOR_CHANNEL = 1,
    AA_VIDEO_CHANNEL = 2,
    AA_TOUCHSCREEN_CHANNEL = 3,
    AA_AUDIO0_CHANNEL = 4,
    AA_AUDIO1_CHANNEL = 5,
    AA_AUDIO2_CHANNEL = 6,
    AA_MIC_CHANNEL = 7,
    AA_MAX_CHANNEL = 7,
};

enum aa_message_type {
    AA_VERSION_REQ = 1,
    AA_VERSION_RESPONSE = 2,

    AA_SSL_HANDSHAKE_DATA = 3,
    AA_SSL_COMPLETE = 4,

    AA_DISCOVERY_REQUEST = 5,
    AA_DISCOVERY_RESPONSE = 6,
    AA_CHANNEL_OPEN_REQUEST = 7,
    AA_CHANNEL_OPEN_RESPONSE = 8,

    AA_NAV_FOCUS_REQUEST = 13,
    AA_NAV_FOCUS_NOTIFY = 14,

    AA_MEDIA_SETUP = 0x8000,
    AA_MEDIA_START_REQUEST = 0x8001,
    AA_SENSOR_START = 0x8002,
    AA_SENSOR_DATA = 0x8003,
    AA_VID_ACK = 0x8004,
    AA_VIDEO_FOCUS_GAINED = 0x8008,

    AA_MEDIA_DATA = 0,
    AA_CODEC_DATA = 1,

    AA_TOUCHSCREEN_INPUT = 0x8001,
};

enum aa_input_actions {
    AA_INPUT_ACTION_MOUSEUP = 1,
    AA_INPUT_ACTION_MOUSEDOWN = 0,
    AA_INPUT_ACTION_CANCEL = 1,
    AA_INPUT_ACTION_MOVE = 2,
};


inline packet_ptr_t make_packet_common(u_char chan, uint16_t msg_type, bool encrypted,
                                       size_t hint = 0) {
    packet_ptr_t res(new packet_t());
    res->chan_ = chan;
    res->encrypted_ = encrypted;
    res->control_ = msg_type <= 0xFF && chan != AA_CONTROL_CHANNEL;
    res->content_.reserve(hint+4);
    res->content_.push_back(safe_cast<u_char>(msg_type / 256));
    res->content_.push_back(safe_cast<u_char>(msg_type % 256));
    return res;
}


inline packet_ptr_t make_packet(u_char chan, uint16_t msg_type, bool encrypted,
                                std::initializer_list<u_char> &&vals) {
    packet_ptr_t p = make_packet_common(chan, msg_type, encrypted, vals.size());
    p->content_.insert(p->content_.end(), vals);
    return p;
}

inline packet_ptr_t make_packet(u_char chan, uint16_t msg_type, bool encrypted,
                                const std::vector<u_char> &data) {
    packet_ptr_t p = make_packet_common(chan, msg_type, encrypted, data.size());
    p->content_.insert(p->content_.end(), data.begin(), data.end());
    return p;
}

inline uint16_t get_msg_type(const std::vector<u_char> &buf) {
    return safe_cast<uint16_t>(buf.at(0)*256 + buf.at(1));
}

inline std::string lookup_name(uint16_t id, bool control_channel=true)
{
    if (!control_channel)
    {
        if (id == 1)
            return "Codec_Data";
        if (id == 0x8001)
            return "SensorMedia_Start_Request";
        if (id == 0x8002)
            return "Pause_Resume_Request";
    }

    switch (id)
    {
        case 0: return "Media_Data";
        case 1: return "Version_Request";
        case 2: return "Version_Response";
        case 3: return "SSL_Handshake_Data";
        case 4: return "SSL_Authentication_Complete_Notification";
        case 5: return "Service_Discovery_Request";
        case 6: return "Service_Discovery_Response";
        case 7: return "Channel_Open_Request";
        case 8: return "Channel_Open_Response";
        case 9: return "9_??";
        case 10: return "10_??";
        case 11: return "Ping_Request";
        case 12: return "Ping_Response";
        case 13: return "Navigation_Focus_Request";
        case 14: return "Navigation_Focus_Notification";
        case 15: return "Byebye_Request";
        case 16: return "Byebye_Response";
        case 17: return "Voice_Session_Notification";
        case 18: return "Audio_Focus_Request";
        case 19: return "Audio_Focus_Notification";
        case 0x8000: return "Media_Setup_Request";
        case 0x8001: return "Touch_Notification";
        case 0x8002: return "Sensor_Start_Request";
        case 0x8004: return "Codec/Media_Data_Ack";
        case 0x8005: return "Mic_Start/Stop_Request";
        case 0x8006: return "k3_6_?";
        case 0x8007: return "Media_Video_?_Request";
        case 0x8008: return "Video_Focus_Notification";
        case 0xFFFF: return "Framing_Error_Notification";
        default: return "AA_Packet";
    }
}

inline std::string desc(const packet_ptr_t &pack)
{
    uint16_t m = get_msg_type(pack->content_);
    std::stringstream content_out;
    for(int f=0;f<32 && f< pack->content_.size();++f)
        content_out << std::hex << (uint)pack->content_[f]/16 << (uint)pack->content_[f]%16;

    str_out_t p;
    p << lookup_name(m) << ":chan=" << (uint)pack->chan_ << ":len="
           << pack->content_.size() << ":data=" << content_out.str();
    return p;
}

inline void encode_varint_to(int64_t val, buf_t &target)
{
    if (val >= 0x7fffffffffffffffL)
        throw std::out_of_range("Value is too big");

    uint64_t left = val;

    for(int idx2=0; idx2<9; ++idx2) {
        target.push_back((u_char)(0x7f & left));
        left >>= 7;
        if (left == 0)
            return;
        if (idx2 < 9 - 1)
            target.back() |= 0x80;
    }
}

#endif //AAUTO_AA_HELPER_H
