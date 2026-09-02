#include<vector>
#include<assert.h>

#include"log.hpp"

#define BUFFER_DEFAULT_SIZE 1024
class Buffer {
    private:
        std::vector<char> _buffer; //使用vector进行内存空间管理
        uint64_t _reader_idx; //读偏移
        uint64_t _writer_idx; //写偏移
    public:
        Buffer():_reader_idx(0), _writer_idx(0), _buffer(BUFFER_DEFAULT_SIZE){}
        char *Begin() { return &*_buffer.begin(); }   
        char *WritePosition() { return Begin() + _writer_idx; }   
        char *ReadPosition() { return Begin() + _reader_idx; }
        uint64_t TailIdleSize() { return _buffer.size() - _writer_idx; }
        uint64_t HeadIdleSize() { return _reader_idx; }
        uint64_t ReadAbleSize() { return _writer_idx - _reader_idx; }
        void MoveReadOffset(uint64_t len) { 
            if (len == 0) return; 
  
            assert(len <= ReadAbleSize());
            _reader_idx += len;
        }
      
        void MoveWriteOffset(uint64_t len) {
       
            assert(len <= TailIdleSize());
            _writer_idx += len;
        }
        
        void EnsureWriteSpace(uint64_t len) {
         
            if (TailIdleSize() >= len) { return; }
        
            if (len <= TailIdleSize() + HeadIdleSize()) {
                
                uint64_t rsz = ReadAbleSize();
                std::copy(ReadPosition(), ReadPosition() + rsz, Begin());
                _reader_idx = 0;    
                _writer_idx = rsz;  
            }else {
              
                DBG_LOG("RESIZE %ld", _writer_idx + len);
                _buffer.resize(_writer_idx + len);
            }
        } 

        void Write(const void *data, uint64_t len) {
            //1. 保证有足够空间，2. 拷贝数据进去
            if (len == 0) return;
            EnsureWriteSpace(len);
            const char *d = (const char *)data;
            std::copy(d, d + len, WritePosition());
        }
        void WriteAndPush(const void *data, uint64_t len) {
            Write(data, len);
            MoveWriteOffset(len);
        }
        void WriteString(const std::string &data) {
            return Write(data.c_str(), data.size());
        }
        void WriteStringAndPush(const std::string &data) {
            WriteString(data);
            MoveWriteOffset(data.size());
        }
        void WriteBuffer(Buffer &data) {
            return Write(data.ReadPosition(), data.ReadAbleSize());
        }
        void WriteBufferAndPush(Buffer &data) { 
            WriteBuffer(data);
            MoveWriteOffset(data.ReadAbleSize());
        }
        //读取数据
        void Read(void *buf, uint64_t len) {
            //要求要获取的数据大小必须小于可读数据大小
            assert(len <= ReadAbleSize());
            std::copy(ReadPosition(), ReadPosition() + len, (char*)buf);
        }
        void ReadAndPop(void *buf, uint64_t len) {
            Read(buf, len);
            MoveReadOffset(len);
        }
        std::string ReadAsString(uint64_t len) {
            //要求要获取的数据大小必须小于可读数据大小
            assert(len <= ReadAbleSize());
            std::string str;
            str.resize(len);
            Read(&str[0], len);
            return str;
        }
        std::string ReadAsStringAndPop(uint64_t len) {
            assert(len <= ReadAbleSize());
            std::string str = ReadAsString(len);
            MoveReadOffset(len);
            return str;
        }
        char *FindCRLF() {
            char *res = (char*)memchr(ReadPosition(), '\n', ReadAbleSize());
            return res;
        }
        /*通常获取一行数据，这种情况针对是*/
        std::string GetLine() {
            char *pos = FindCRLF();
            if (pos == NULL) {
                return "";
            }
            // +1是为了把换行字符也取出来。
            return ReadAsString(pos - ReadPosition() + 1);
        }
        std::string GetLineAndPop() {
            std::string str = GetLine();
            MoveReadOffset(str.size());
            return str;
        }
        //清空缓冲区
        void Clear() {
            //只需要将偏移量归0即可
            _reader_idx = 0;
            _writer_idx = 0;
        }
};