//
// Created by lic on 11/1/21.
//
#ifndef ERIZO_SRC_ERIZO_RTP_CROPFILTER_H_
#define ERIZO_SRC_ERIZO_RTP_CROPFILTER_H_

#include "pipeline/Handler.h"
#include "./logger.h"
#include "../MediaDefinitions.h"
#include <string>
#include "../media/Depacketizer.h"
#include "../media/codecs/VideoCodec.h"
#include "../media/codecs/Codecs.h"
#include <iostream>
#include <fstream>
#include "rtp/RtpHeaders.h"
#include "rtp/RtpVP8Fragmenter.h"
#include <time.h>

extern "C" {
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfiltergraph.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
}

namespace erizo {

    class CropFilter : public CustomHandler{
        DECLARE_LOGGER();
    public:
        CropFilter(std::vector<std::string> parameters);
        ~CropFilter();

        void enable() override;
        void disable() override;

        std::string getName() override {
            return "CropFilter";
        }

        void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
        void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
        void notifyUpdate() override;
        int position () override;
    private:
        std::vector<std::string> parameters; //Parameters recieved
        AVFilterContext *buffersink_ctx; //Context for buffer sink: Were we send frames to be filtered
        AVFilterContext *buffersrc_ctx; //ontext for buffer source: Were we receive frames filtered
        AVFilterContext *crop_ctx;  //Context for crop filter
        AVFilterContext *resize_ctx; //Context for resize filfer
        AVFilterGraph *filter_graph; // Context for filter graph
        AVFrame *filt_frame; //Frame filtered
        AVFrame *frame; //Frame decoded
        Vp8Depacketizer* dpckg; //Depacketizer: rtp packets to frames
        bool last_frame = false; //Frame available from depacketizer TODO:Chamge name
        char args[1024]; //Args for filters
        VideoDecoder vDecoder; //Video decoder
        VideoEncoder vEncoder; //Video encoder
        VideoCodecInfo vEncodeInfo;  //Video encoder info
        VideoCodecInfo vDecodeInfo;//Video decoder info
        int gotFrame = 0; //Got frame from decoder
        boost::scoped_array<unsigned char> outBuff; //Buffer from exit of decoder
        int outBuffLen=320*240*3/2; //Size of the buffer, number of pixel *1.5 because of YUV420P format
        unsigned char* encodeFrameBuff; //Buffer for encoded frame
        int encodeFrameBuffLen = 100000; //Size
        int filtFrameLenght; //Size of the filtered frame
        void display_frame(const AVFrame *frame, int fileN);
        std::FILE *dump,*dump2; //Files to debug decoded and filtered frame
        std::ofstream latency;
        bool encoderInit; //Is encoder init
        unsigned char* fragmenterBuffer; //Buffer for the exit of the rtp fragmenter
        unsigned int lengthFrag; //Lenght of the fragment
        bool lastFragPacket; //IS the last fragment of the packet
        unsigned int seqnum_; //First sequence number recieved
        unsigned int ssrc; // SSRC received
        bool firstPackage;//First package recieved? -> Copy seqnumber
        RtpVP8Parser vp8_parser_; //Parser to fill some
        unsigned char* rtpBuffer_; //buffer for data to send in rtp packet
        unsigned char* filtFrameBuffer; //Frame where we copy YUV planes before sending to encoder
        int numberPixels; //Number of pixel in filt frame for copy planes
        clock_t last_time;
        clock_t current_time;
        double duration;
        double maxDuration=0;
        double maxEncodeDuration=0;
        double maxFilterDuration=0;
        double maxDecodeDuration=0;
        double minDuration=999999;
        double minEncodeDuration=999999;
        double minFilterDuration=999999;
        double minDecodeDuration=999999;
        long double totalEncode=0;
        long double totalDecode=0;
        long double totalFilter=0;
        double total=0;
        int count=0;
    };
}
#endif //ERIZO_SRC_ERIZO_RTP_CROPFILTER_H_
