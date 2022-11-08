#include "BoundedBuffer.h"

using namespace std;


BoundedBuffer::BoundedBuffer (int _cap) : cap(_cap) {
    // modify as needed
}

BoundedBuffer::~BoundedBuffer () {
    // modify as needed
}

void BoundedBuffer::push (char* msg, int size) {
    // 1. Convert the incoming byte sequence given by msg and size into a vector<char>
    char* info = (char *) msg;
    vector<char> inf (info, info + size);

    // 2. Wait until there is room in the queue (i.e., queue lengh is less than cap)
    unique_lock<mutex> l(m);
    slot.wait(l, [this]{return (int) q.size() < cap;});

    // 3. Then push the vector at the end of the queue
    q.push(inf);
    l.unlock();

    // 4. Wake up threads that were waiting for push
    data.notify_one();
}

int BoundedBuffer::pop (char* msg, int size) {
    // 1. Wait until the queue has at least 1 item
    unique_lock<mutex> l(m);
    data.wait(l, [this]{return q.size() > 0;});

    // 2. Pop the front item of the queue. The popped item is a vector<char>
    vector<char> inf = q.front();
    q.pop();
    l.unlock();

    // 3. Convert the popped vector<char> into a char*, copy that into msg; assert that the vector<char>'s length is <= size
    assert((int) inf.size() <= size);
    memcpy(msg, inf.data(), inf.size());

    // 4. Wake up threads that were waiting for pop
    slot.notify_one();

    // 5. Return the vector's length to the caller so that they know how many bytes were popped
    return inf.size();
}

size_t BoundedBuffer::size () {
    return q.size();
}
