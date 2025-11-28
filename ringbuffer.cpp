#include "ringbuffer.h"

#include <algorithm>
#include <cstring>

RingBuffer::RingBuffer(size_t capacity)
    : m_capacity(capacity),
      m_buffer(std::make_unique<char[]>(capacity))
{
}

bool RingBuffer::write(const QByteArray &data) {
    const size_t writeLen = static_cast<size_t>(data.size());
    if (writeLen == 0) return true;

    std::lock_guard<std::mutex> lock(m_mutex);
    if (writeLen > freeSpaceUnlocked()) {
        return false;
    }

    const size_t tailSpace = m_capacity - m_tail;
    if (writeLen <= tailSpace) {
        memcpy(m_buffer.get() + m_tail, data.constData(), writeLen);
    } else {
        memcpy(m_buffer.get() + m_tail, data.constData(), tailSpace);
        memcpy(m_buffer.get(), data.constData() + tailSpace, writeLen - tailSpace);
    }
    m_tail = (m_tail + writeLen) % m_capacity;
    return true;
}

bool RingBuffer::read(QByteArray &out, size_t len) {
    if (len == 0) {
        out.clear();
        return true;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    if (len > sizeUnlocked()) {
        return false;
    }

    out.resize(static_cast<int>(len));
    if (m_head + len <= m_capacity) {
        memcpy(out.data(), m_buffer.get() + m_head, len);
    } else {
        const size_t firstPart = m_capacity - m_head;
        memcpy(out.data(), m_buffer.get() + m_head, firstPart);
        memcpy(out.data() + firstPart, m_buffer.get(), len - firstPart);
    }
    m_head = (m_head + len) % m_capacity;
    return true;
}

size_t RingBuffer::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return sizeUnlocked();
}

char RingBuffer::peek(size_t offset) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (sizeUnlocked() == 0) {
        return 0;
    }
    const size_t pos = (m_head + offset) % m_capacity;
    return m_buffer[pos];
}

size_t RingBuffer::freeSpace() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return freeSpaceUnlocked();
}

void RingBuffer::skip(size_t len) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const size_t available = sizeUnlocked();
    if (available == 0) return;

    if (len > available) {
        len = available;
    }
    m_head = (m_head + len) % m_capacity;
}

void RingBuffer::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_head = 0;
    m_tail = 0;
}

size_t RingBuffer::sizeUnlocked() const {
    return (m_tail >= m_head) ?
           (m_tail - m_head) :
           (m_capacity - m_head + m_tail);
}

size_t RingBuffer::freeSpaceUnlocked() const {
    // Keep one byte empty to distinguish full vs empty.
    return m_capacity - sizeUnlocked() - 1;
}
