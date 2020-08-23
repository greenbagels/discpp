/*  This file is part of discpp.
 *
 *  discpp is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  discpp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with discpp. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef QUEUE_HPP
#define QUEUE_HPP

#include <algorithm>
#include <condition_variable>
#include <queue>
#include <mutex>
#include <type_traits>
#include <utility>

namespace discpp
{
    namespace queue
    {
        template <typename T>
        class message_queue
        {
            public:
                /* Suppose the following happens:
                 * --------------------------------------------------
                 *         Thread 1         |       Thread 2
                 * --------------------------------------------------
                 *  T& foo = queue.front()  |         ...
                 *            ...           |      queue.pop()
                 *       T bar = foo        |         ...
                 * --------------------------------------------------
                 *
                 * Then foo becomes a dangling reference, and we have to
                 * manually make sure the calls don't get interleaved. This
                 * is effectively impossible without additional synchronization,
                 * so we will explicitly leave out the reference overload.
                 */

                /*! Returns a copy of the front element of the underlying queue.
                 *
                 *  This is the only overload of front() that we offer, as any
                 *  intermediate reference could be easily invalidated by other
                 *  threads.
                 *
                 *  Exception-safety: Strong guarantee, will throw if cannot
                 *  acquire the lock, or if copy construction fails, leaving the
                 *  class in an unchanged state.
                 */
                T front()
                {
                    std::lock_guard<std::mutex> g(_mutex);
                    return _queue.front();
                }

                /*! Pops the underlying queue
                 *
                 *  Exception-safety: Strong guarantee, will throw if cannot
                 *  acquire the lock
                 */
                void pop()
                {
                    std::lock_guard<std::mutex> g(_mutex); // strong guarantee
                    _queue.pop(); // no-throw guarantee
                }

                /*! Pops the underlying queue onto a given reference variable
                 *
                 *  Exception-safety: Strong guarantee, will throw if cannot
                 *  acquire the lock.
                 */
                void pop(T& ret)
                {
                    // Make sure swap can't throw
                    static_assert(std::is_nothrow_move_constructible<T>::value &&
                            std::is_nothrow_move_assignable<T>::value,
                            "Cannot guarantee no-throw swap for deduced message type!");
                    // Keep any other threads from racing with us
                    std::lock_guard<std::mutex> g(_mutex); // strong guarantee

                    // Since the pop operation cannot throw, we can avoid
                    // copying the first element and directly swap with our
                    // return variable argument.

                    std::swap(_queue.front(), ret);
                    _queue.pop();
                }

                /*! Pushes a value onto the underlying queue (reference overload)
                 *
                 *  Exception-safety: Strong guarantee, will throw if cannot
                 *  acquire the lock, or the underlying push fails.
                 */
                void push(const T& value)
                {
                    std::lock_guard<std::mutex> g(_mutex); // strong guarantee
                    _queue.push(value); // strong guarantee
                    _cvar.notify_all();
                }

                /*! Pushes a value onto the underlying queue (rvalue reference overload)
                 *
                 *  Exception-safety: Strong guarantee, will throw if cannot
                 *  acquire the lock, or the underlying push fails.
                 */
                void push(T&& value)
                {
                    std::lock_guard<std::mutex> g(_mutex); // strong guarantee
                    _queue.push(value); // strong guarantee
                    _cvar.notify_all();
                }

                // TODO: implement swap

                /*
                std::mutex& mutex() const
                {
                    return _mutex;
                }
                */

                bool empty()
                {
                    std::lock_guard<std::mutex> g(_mutex); // strong guarantee
                    return _queue.empty();
                }

                void wait_until_empty()
                {
                    std::unique_lock<std::mutex> g(_mutex);
                    // we HAVE the lock

                    // equivalent to wait(unique_lock, predicate) call, but more
                    // clear imo
                    while (_queue.empty())
                    {
                        // remember: the lock is dropped while _cvar waits
                        _cvar.wait(g);
                        // ...and is reacquired here
                    }
                }

            private:
                /*! Our underlying message queue container */
                std::queue<T> _queue;
                /*! The mutex used by our member functions to ensure thread-safety */
                std::mutex _mutex;
                std::condition_variable _cvar;
        };

    } // namespace queue
} // namespace discpp

#endif
