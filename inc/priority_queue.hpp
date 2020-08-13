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

#ifndef PRIORITY_QUEUE_HPP
#define PRIORITY_QUEUE_HPP

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

        /*! A class satisfying the Compare named requirement that outputs the
         * element of a given pair with a later response deadline.
         */
        template <typename T>
        class LaterDeadline
        {
            public:
            constexpr bool operator()( const T& lhs, const T& rhs ) const
            {
                using std::get;
                // T should have the same interface as std::pair<json, boost::optional<time_point>>
                // The first element is a payload, and the second element is a
                // timestamp for the deadline (if any)
                auto ldate = get<1>(lhs);
                auto rdate = get<1>(rhs);

                /* We're basically implementing operator< for the priority of each
                 * element. Our priority ordering is based on deadline (i.e. highest
                 * priority = smaller deadline).
                 *
                 * In an efficient world, we'd never miss a deadline, but we do have
                 * to eventually decide on how to treat missed deadlines --- late
                 * tasks can cause delays to cascade along the entire queue, at the
                 * trade-off of potentially catching up. Meanwhile, an overdue task
                 * could *still* be important to service, so outright stripping its
                 * priority could accidentally cause a critical error!
                 */

                /* Note that the way operator< is defined for optional arguments:
                 * None < None == false
                 * Some < None == false
                 * None < Some == true
                 * Some < Some == *Some < *Some
                 *
                 * But since smaller chrono timepoints are higher priority, we need
                 * to reverse the comparison in the last case.
                 */

                if (ldate && rdate)
                {
                    // Reverse it
                    return rdate > ldate;
                }

                // We have a null value, so regular comparison will give us the
                // higher priority event
                return ldate > rdate;
            }
        };

        template <typename T>
        class priority_message_queue
        {
            public:
                /* Suppose the following happens:
                 * --------------------------------------------------
                 *         Thread 1         |       Thread 2
                 * --------------------------------------------------
                 *  T& foo = queue.top()    |         ...
                 *            ...           |      queue.pop()
                 *       T bar = foo        |         ...
                 * --------------------------------------------------
                 *
                 * Then foo becomes a dangling reference, and we have to
                 * manually make sure the calls don't get interleaved. This
                 * is effectively impossible without additional synchronization,
                 * so we will explicitly leave out the reference overload.
                 */

                /*! Returns a copy of the top element of the underlying queue.
                 *
                 *  This is the only overload of front() that we offer, as any
                 *  intermediate reference could be easily invalidated by other
                 *  threads.
                 *
                 *  Exception-safety: Strong guarantee, will throw if cannot
                 *  acquire the lock, or if copy construction fails, leaving the
                 *  class in an unchanged state.
                 */
                T top()
                {
                    std::lock_guard<std::mutex> g(_mutex);
                    return _queue.top();
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
                    auto top = _queue.top();

                    std::swap(top, ret);
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
                std::priority_queue<T, std::vector<T>, LaterDeadline<T>> _queue;
                /*! The mutex used by our member functions to ensure thread-safety */
                std::mutex _mutex;
                std::condition_variable _cvar;
        };

    } // namespace queue
} // namespace discpp

#endif
