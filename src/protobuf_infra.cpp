#include <protobuf_infra.h>

bool FileRead_callback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
    File *file = (File *)stream->state;

    if (buf == nullptr)
    {
        bool available = file->available();
        file->seek(count, SeekCur);
        return (available == count);
    }

    size_t bytesRead = file->read(buf, count);
    return bytesRead == count;
}

pb_istream_t FileToPbStream(File &f)
{
    pb_istream_t stream = {&FileRead_callback, &f, SIZE_MAX};
    stream.bytes_left = f.available();
    return stream;
}

bool StreamRead_callback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
    Stream *sourceStream= (Stream *)stream->state;

    if (buf == nullptr) // consume bytes wiutout doing aynthing with them
    {
        for(int i=0; i<count; i++) {
            int res = sourceStream->read();
            if(res < 0) {
                return false;
            }
        }
        return true;
    }

    size_t bytesRead = sourceStream->readBytes(buf, count);
    return bytesRead == count;
}

pb_istream_t StreamToPbStream(Stream *s, size_t totalSize)
{
    pb_istream_t stream = {&StreamRead_callback, s, totalSize};
    return stream;
}
