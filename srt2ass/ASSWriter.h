#ifndef ASSWriter_H
#define ASSWriter_H


#include "srtTransfer.h"
#include <string_view>
#include <string>


struct ASSWriterError {
    ASSWriterError(const char* msg) : msg_(msg) {}
    const char* what() const {return msg_;}
    private:
        const char* msg_;
};

struct FileWriteException : ASSWriterError {
    FileWriteException(const char* msg, const std::string_view& path) 
        : ASSWriterError(msg), path_(std::string(path)) {}
    const std::string& path() const { return path_; }
    private:
        std::string path_;
};


using AssSubtitleEntry = srtTransfer::AssSubtitleEntry;


class ASSWriter {

    public:
        ASSWriter() = delete;
        ASSWriter(const std::string_view path, const std::vector<AssSubtitleEntry>& ass_data) : 
            __path(path), __ass_data(ass_data) {}
        void write();

    public:
        const std::string_view& getPath() {return __path;}
        const std::vector<AssSubtitleEntry>& getEntry() {return __ass_data;}

    private:
        const std::string_view __path;
        const std::vector<AssSubtitleEntry> __ass_data;
};

#endif
