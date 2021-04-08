//
// Created by lic on 11/1/21.
//
#ifndef ERIZO_SRC_ERIZO_RTP_CROPFILTER_H_
#define ERIZO_SRC_ERIZO_RTP_CROPFILTER_H_

#include "../pipeline/Handler.h"
#include "./logger.h"
#include "../media/Depacketizer.h"
#include "../media/codecs/VideoCodec.h"
#include "../media/codecs/Codecs.h"
#include "../rtp/RtpHeaders.h"
#include "../rtp/RtpVP8Fragmenter.h"
#include "../MediaDefinitions.h"
#include "rtp/RtpUtils.h"
#include <iostream>
#include <fstream>
#include <string>

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
        CropFilter(std::map <std::string,std::string>  parameters);
        ~CropFilter();

        void enable() override;
        void disable() override;

        std::string getName() override {
            return "CropFilter";
        }

        void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
        void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
        void notifyUpdate() override;
        Positions position () override;
    private:
        void configureFilters(int width, int height, std::string cropConfig);
        std::map <std::string,std::string>  parameters; //Parameters recieved
        AVFilterContext *buffersink_ctx; //Context for buffer sink: Were we send frames to be filtered
        AVFilterContext *buffersrc_ctx; //ontext for buffer source: Were we receive frames filtered
        AVFilterContext *crop_ctx;  //Context for crop filter
        AVFilterContext *resize_ctx; //Context for resize filfer
        AVFilter *buffersrc;
        AVFilter *buffersink;
        AVFilter *crop;
        AVFilter *resize ;
        AVFilterGraph *filter_graph; // Context for filter graph
        AVFrame *filt_frame; //Frame filtered
        AVFrame *frame; //Frame decoded
        Vp8Depacketizer* dpckg; //Depacketizer: rtp packets to frames
        bool gotFrameFromPckg = false; //Frame available from depacketizer
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
        RtpHeader *copy_head;
        RtcpHeader *rtcp_head;
        bool sizeChanged = false;
        int lastHeight = 320;
        int lastWidth = 240;
};
}
#endif //ERIZO_SRC_ERIZO_RTP_CROPFILTER_H_
