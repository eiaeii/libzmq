/*
    Copyright (c) 2007-2010 iMatix Corporation

    This file is part of 0MQ.

    0MQ is free software; you can redistribute it and/or modify it under
    the terms of the Lesser GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    0MQ is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    Lesser GNU General Public License for more details.

    You should have received a copy of the Lesser GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "../include/zmq.h"

#include "lb.hpp"
#include "pipe.hpp"
#include "err.hpp"

zmq::lb_t::lb_t () :
    active (0),
    current (0)
{
}

zmq::lb_t::~lb_t ()
{
    for (pipes_t::size_type i = 0; i != pipes.size (); i++)
        pipes [i]->term ();
}

void zmq::lb_t::attach (writer_t *pipe_)
{
    pipes.push_back (pipe_);
    pipes.swap (active, pipes.size () - 1);
    active++;
}

void zmq::lb_t::detach (writer_t *pipe_)
{
    //  Remove the pipe from the list; adjust number of active pipes
    //  accordingly.
    if (pipes.index (pipe_) < active) {
        active--;
        if (current == active)
            current = 0;
    }
    pipes.erase (pipe_);
}

void zmq::lb_t::revive (writer_t *pipe_)
{
    //  Move the pipe to the list of active pipes.
    pipes.swap (pipes.index (pipe_), active);
    active++;
}

int zmq::lb_t::send (zmq_msg_t *msg_, int flags_)
{
    while (active > 0) {
        if (pipes [current]->write (msg_))
            break;

        active--;
        if (current < active)
            pipes.swap (current, active);
        else
            current = 0;
    }

    //  If there are no pipes we cannot send the message.
    if (active == 0) {
        errno = EAGAIN;
        return -1;
    }

    if (!(flags_ & ZMQ_NOFLUSH))
        pipes [current]->flush ();

    //  Detach the message from the data buffer.
    int rc = zmq_msg_init (msg_);
    zmq_assert (rc == 0);

    //  Move to the next pipe (load-balancing).
    current = (current + 1) % active;

    return 0;
}

bool zmq::lb_t::has_out ()
{
    while (active > 0) {
        if (pipes [current]->check_write ())
            return true;

        active--;
        if (current < active)
            pipes.swap (current, active);
        else
            current = 0;
    }

    return false;
}

