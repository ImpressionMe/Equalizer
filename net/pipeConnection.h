
/* Copyright (c) 2005, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#ifndef EQNET_PIPE_CONNECTION_H
#define EQNET_PIPE_CONNECTION_H

#include "fdConnection.h"

namespace eqNet
{
    /**
     * A fork-based pipe connection.
     */
    class PipeConnection : public FDConnection
    {
    public:
        PipeConnection(ConnectionDescription &description);
        virtual ~PipeConnection();

        virtual bool connect();
        virtual void close();

    protected:

    private:
        void _createPipes();

        void _setupParent();
        void _runChild();

        int *_pipes;
    };
};

#endif //EQNET_PIPE_CONNECTION_H
