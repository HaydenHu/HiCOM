#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <QByteArray>
#include <mutex>
#include <memory>

class RingBuffer {
public:
    explicit RingBuffer(size_t capacity);

    bool write(const QByteArray &data);
    bool read(QByteArray &out, size_t len);
    size_t size() const;
    char peek(size_t offset) const;
    size_t freeSpace() const;
    void skip(size_t len);
    void clear();

private:
    size_t sizeUnlocked() const;
    size_t freeSpaceUnlocked() const;

    mutable std::mutex m_mutex;
    size_t m_capacity = 0;
    std::unique_ptr<char[]> m_buffer;
    size_t m_head = 0;
    size_t m_tail = 0;
};

#endif // RINGBUFFER_H
