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
        std::vector<std::string> parameters;
        AVFilterContext *buffersink_ctx;
        AVFilterContext *buffersrc_ctx;
        AVFilterContext *crop_ctx;
        AVFilterGraph *filter_graph;
        AVPacket packet;
        AVFrame *frame;
        AVFrame *filt_frame;
        AVFilterInOut *outputs;
        AVFilterInOut *inputs;
        Vp8Depacketizer* dpckg;
        AVFormatContext* context_;
        AVPacket av_packet;
        const AVCodec *codec;
        AVCodecParserContext *parser;
        AVCodecContext *c;
        bool last_frame = false;
        char args[1024];
        boost::scoped_array<unsigned char> outBuff;
        VideoDecoder vDecoder;
        int gotFrame = 0;
        int outBuffLen=320*240*3/2;
        void display_frame(const AVFrame *framem, int fileN);
        std::FILE *dump,*dump2;
    };
}
#endif //ERIZO_SRC_ERIZO_RTP_CROPFILTER_H_
