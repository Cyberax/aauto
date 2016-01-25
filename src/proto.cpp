//
// Created by Besogonov, Aleksei on 1/26/16.
//

#include "proto.h"
#include "aa_helpers.h"
#include "decoder.h"

const static std::vector<u_char> discovery_data={
    // CH 1 Sensors:                      //cq/co[]
    0x0A, 4 + 4*1,  // co: int, cm/cn[]
                  0x08, AA_SENSOR_CHANNEL,
                  0x12, 4*1,
                            //0x0A, 2, 0x08, 7,   // SENSOR_TYPE_GEAR           6
                            0x0A, 2, 0x08, 11,  // SENSOR_TYPE_DRIVING_STATUS 12
    // CH 2 Video Sink:
    0x0A, 4+4+11, 0x08, AA_VIDEO_CHANNEL,
                  0x1A, 4+11,  // Sink: Video
                            0x08, 3,    // int (codec type) 3 = Video
                            0x22, 11,   // Res        FPS, WidMar, HeiMar, DPI
                            0x08, 1, 0x10, 1, 0x18, 0, 0x20, 0, 0x28, -96 & 0xFF, 1,   // 800x 480, 30 fps, 0, 0, 160 dpi    0xa0 // Default 160 like 4100NEX
                            // 0x08, 1, 0x10, 1, 0x18, 0, 0x20, 0, 0x28, -128, 1,   //0x30, 0,     //  800x 480, 30 fps, 0, 0, 128 dpi    0x80 // 160-> 128 Small, phone/music close to outside
                            // 0x08, 1, 0x10, 1, 0x18, 0, 0x20, 0, 0x28,  -16, 1,   //0x30, 0,     //  800x 480, 30 fps, 0, 0, 240 dpi    0xf0 // 160-> 240 Big, phone/music close to center
                            // 60 FPS makes little difference:
                            // 0x08, 1, 0x10, 2, 0x18, 0, 0x20, 0, 0x28,  -96, 1,   //0x30, 0,     //  800x 480, 60 fps, 0, 0, 160 dpi    0xa0
                            // Higher resolutions don't seem to work as of June 10, 2015 release of AA:
                            // 0x08, 2, 0x10, 1, 0x18, 0, 0x20, 0, 0x28,  -96, 1,   //0x30, 0,     // 1280x 720, 30 fps, 0, 0, 160 dpi    0xa0
                            // 0x08, 3, 0x10, 1, 0x18, 0, 0x20, 0, 0x28,  -96, 1,   //0x30, 0,     // 1920x1080, 30 fps, 0, 0, 160 dpi    0xa0
    // Crashes on null Point reference without:
    0x0A, 4+2+6,  // CH 3 TouchScreen/Input:
                0x08, AA_TOUCHSCREEN_CHANNEL,
                0x22, 2+6,
                        0x12, 6,
                        0x08, -96 & 0xFF, 6, 0x10, -32 & 0xFF, 3, // 800x 480
                        // 0x08, -128, 10,    0x10, -48, 5,       // 1280x 720     0x80, 0x0a   0xd0, 5
                        // 0x08, -128, 15,    0x10, -72, 8,       // 1920x1080     0x80, 0x0f   0xb8, 8

    0x12, 4, 'T', 'S', 'L', 'A',  // Car Manuf          Part of "remembered car"
    0x1A, 4, 'M', 'D', 'L', 'S',  // Car Model
    0x22, 4, '2', '0', '1', '6',  // Car Year           Part of "remembered car"
    0x2A, 4, '0', '0', '0', '1',  // Car Serial     Not Part of "remembered car" ??     (vehicleId=null)
    0x30, 1,  // driverPosition
    0x3A, 4, 'A', 'l', 'e', 'x',  // HU  Make / Manuf
    0x42, 4, 'T', 'S', 'L', 'A',  // HU  Model
    0x4A, 4, 'B', 'L', 'D', '1',  // HU  SoftwareBuild
    0x52, 4, 'V', '0', '0', '1',  // HU  SoftwareVersion
    0x58, 0,  // ? bool (or int )    canPlayNativeMediaDuringVr
    0x60, 0,  // mHideProjectedClock     1 = True = Hide
};

void proto_t::run_loop() {
    while(terminator_->check_termination())
    {
        // Run the protocol state machine
        if (phase_ == INIT) {
            transit_to(VERSION_NEGO);
            TA_DEBUG() << "Sending version negotiation";
            this->trans_->write_packet(make_packet(0, AA_VERSION_REQ, false, {0, 1, 0, 1}));
            continue;
        }
        
        if (phase_ == VERSION_NEGO && (this->phase_start_+VERSION_NEGO) < time(NULL))
        {
            // It's taking too long to get the reply, retry it
            transit_to(INIT);
            TA_DEBUG() << "Version reply timeout";
            continue;
        }

        packet_ptr_t packet = this->trans_->handle_events();
        if (!packet)
            continue;
        if (!packet->encrypted_)
            TA_TRACE() << "Received: " << desc(packet);

        if (phase_ == VERSION_NEGO)
        {
            if (get_msg_type(packet->content_) != AA_VERSION_RESPONSE) {
                transit_to(INIT);
                TA_INFO() << "Received wrong response to version request " << desc(packet);
                continue;
            }
            
            //Write the nego packet
            buf_t data = crypto_->do_handshake(buf_t(), 0);
            this->trans_->write_packet(make_packet(AA_CONTROL_CHANNEL, AA_SSL_HANDSHAKE_DATA,
                                                   false, data));
            transit_to(SSL_HANDSHAKE);
            continue;
        }

        if (phase_ == SSL_HANDSHAKE)
        {
            if (get_msg_type(packet->content_) != AA_SSL_HANDSHAKE_DATA) {
                TA_INFO() << "Unexpected packet received during SSL nego: " << desc(packet);
                transit_to(INIT);
                continue;
            }

            buf_t response = this->crypto_->do_handshake(packet->content_, 2);
            if (response.empty())
            {
                if (!this->crypto_->is_handshake_finished())
                {
                    TA_INFO() << "Unexpected packet received during SSL nego: " << desc(packet);
                    transit_to(INIT);
                    continue;
                }

                this->trans_->write_packet(make_packet(AA_CONTROL_CHANNEL, AA_SSL_COMPLETE,
                                                       false, {0x08, 0}));
                transit_to(READY);
                continue;
            } else {
                this->trans_->write_packet(make_packet(AA_CONTROL_CHANNEL, AA_SSL_HANDSHAKE_DATA,
                                                       false, response));
                continue;
            }
        }
        
        if (phase_ == READY)
        {
            //Decrypt the packet
            buf_t plain = this->crypto_->decrypt(packet->content_, 0);
            packet->content_.swap(plain);
            TA_TRACE() << "Decrypted packet: " << desc(packet);

            dispatch_in_established(packet);
        }
    }
}

void proto_t::encrypt_and_send(packet_ptr_t pack)
{
    packet_ptr_t enc_packet(new packet_t());
    enc_packet->chan_ = pack->chan_;
    enc_packet->control_ = pack->control_;
    enc_packet->encrypted_ = true;

    buf_t enc = this->crypto_->encrypt(pack->content_, 0);
    enc_packet->content_.swap(enc);
    this->trans_->write_packet(enc_packet);
}

void proto_t::dispatch_in_established(packet_ptr_t pack)
{
    uint16_t msg_type = get_msg_type(pack->content_);
    switch (msg_type)
    {
        case AA_DISCOVERY_REQUEST:
            encrypt_and_send(make_packet(AA_CONTROL_CHANNEL, AA_DISCOVERY_RESPONSE, true,
                                         discovery_data));
        break;
        case AA_CHANNEL_OPEN_REQUEST:
            encrypt_and_send(make_packet(pack->chan_, AA_CHANNEL_OPEN_RESPONSE, true, {8, 0}));
            // We're parked!!!
            if (pack->chan_ == AA_SENSOR_CHANNEL)
                encrypt_and_send(make_packet(pack->chan_, AA_SENSOR_DATA, true, {0x6a, 2, 8, 0}));
        break;
        case AA_MEDIA_SETUP:
            encrypt_and_send(make_packet(pack->chan_, AA_SENSOR_DATA, true,
                                         {0x08, 2, 0x10, 48, 0x18, 0}));
            if (pack->chan_ == AA_VIDEO_CHANNEL)
                encrypt_and_send(make_packet(pack->chan_, AA_VIDEO_FOCUS_GAINED, true,
                                             {0x08, 1, 0x10, 1}));
        break;
        case AA_SENSOR_START:
            encrypt_and_send(make_packet(pack->chan_, AA_SENSOR_DATA, true, {8, 0}));
        break;
        case AA_MEDIA_START_REQUEST:
            if (pack->chan_ == AA_SENSOR_CHANNEL) {
                encrypt_and_send(make_packet(pack->chan_, AA_CHANNEL_OPEN_RESPONSE, true, {8, 0}));
                encrypt_and_send(make_packet(pack->chan_, AA_SENSOR_DATA, true, {0x6a, 2, 8, 0}));
            }
        break;
        case AA_MEDIA_DATA:
            if (pack->chan_ == AA_VIDEO_CHANNEL)
                encrypt_and_send(make_packet(pack->chan_, AA_VID_ACK, true, {0x08, 0, 0x10,  1}));
            //Fall-through
        case AA_CODEC_DATA:
            if (pack->chan_ == AA_VIDEO_CHANNEL) {
                decoder_->check_for_errors();
                decoder_->submit_packet(pack);
            }
        break;
        case AA_NAV_FOCUS_REQUEST:
            encrypt_and_send(make_packet(pack->chan_, AA_NAV_FOCUS_NOTIFY, true, {0x08, 2}));
        break;
        default:
            TA_INFO() << "Unknown packet: " << desc(pack);
    }
}

void proto_t::notify_mouse(int x, int y, bool mouse_down)
{
    buf_t coords;
    coords.push_back(0x08);  // Value 1
    encode_varint_to(x, coords);
    coords.push_back(0x10); // Value 2
    encode_varint_to(y, coords);
    coords.push_back(0x18);  // Value 3
    coords.push_back(0);  // Encode Z ?

    buf_t tevent;
    tevent.push_back(0x0a);  // Value 3 array
    tevent.push_back((u_char) coords.size());
    tevent.insert(tevent.end(), coords.begin(), coords.end());

    tevent.push_back(0x10);
    tevent.push_back(0x00);
    tevent.push_back(0x18);
    tevent.push_back(mouse_down?AA_INPUT_ACTION_MOUSEDOWN:AA_INPUT_ACTION_MOUSEUP);

    buf_t ba_touch;
    //  Encode timestamp
    // Timestamp is in nanoseconds
    uint64_t ts = 0;
    ba_touch.push_back(0x08);  // Integer
    encode_varint_to(ts, ba_touch);

    ba_touch.push_back(0x1a);  // Contents = 1 array
    ba_touch.push_back((u_char) tevent.size());
    ba_touch.insert(ba_touch.end(), tevent.begin(), tevent.end());

    encrypt_and_send(make_packet(AA_TOUCHSCREEN_CHANNEL, AA_TOUCHSCREEN_INPUT, true,
                                 ba_touch));
}
