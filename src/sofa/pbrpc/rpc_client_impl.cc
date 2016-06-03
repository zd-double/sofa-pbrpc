// Copyright (c) 2014 Baidu.com, Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Author: qinzuoyan01@baidu.com (Qin Zuoyan)

#include <algorithm>
#include <deque>

#include <sofa/pbrpc/rpc_client_impl.h>
#include <sofa/pbrpc/tran_buf_pool.h>
#include <sofa/pbrpc/flow_controller.h>
#include <sofa/pbrpc/compressed_stream.h>
#include <sofa/pbrpc/murmurhash.h>

namespace sofa {
namespace pbrpc {

RpcClientImpl::RpcClientImpl(const RpcClientOptions& options)
    : _options(options)
    , _is_running(false)
    , _next_request_id(0)
    , _epoch_time(ptime_now())
    , _ticks_per_second(time_duration_seconds(1).ticks())
    , _last_maintain_ticks(0)
    , _last_print_connection_ticks(0)
    , _slot_head(NULL)
{
    _slice_count = std::max(1, 1000 / MAINTAIN_INTERVAL_IN_MS);
    _slice_quota_in = _options.max_throughput_in == -1 ?
        -1 : std::max(0L, _options.max_throughput_in * 1024L * 1024L) / _slice_count;
    _slice_quota_out = _options.max_throughput_out == -1 ?
        -1 : std::max(0L, _options.max_throughput_out * 1024L * 1024L) / _slice_count;
    _max_pending_buffer_size =
        std::max(0L, _options.max_pending_buffer_size * 1024L * 1024L);
    _keep_alive_ticks = _options.keep_alive_time == -1 ?
        -1 : std::max(1, _options.keep_alive_time) * _ticks_per_second;
    _multi_connection_count = std::max(1, _options.multi_connection_count);
    _connection_type = _multi_connection_count > 1 ?
        MULTI_CONNECTION : SINGLE_CONNECTION;

    memset(_stream_slots, 0, sizeof(_stream_slots));

#if defined( LOG )
    LOG(INFO) << "RpcClientImpl(): quota_in="
              << (_slice_quota_in == -1 ? -1 : _slice_quota_in * _slice_count / (1024L * 1024L))
              << "MB/s, quota_out="
              << (_slice_quota_out == -1 ? -1 : _slice_quota_out * _slice_count / (1024L * 1024L))
              << "MB/s, max_pending_buffer_size="
              << (_max_pending_buffer_size / (1024L * 1024L))
              << "MB, keep_alive_time="
              << (_keep_alive_ticks == -1 ? -1 : _keep_alive_ticks / _ticks_per_second)
              << "seconds";
#else
    SLOG(INFO, "RpcClientImpl(): quota_in=%lldMB/s, quota_out=%lldMB/s"
            ", max_pending_buffer_size=%lldMB, keep_alive_time=%lldseconds",
            _slice_quota_in == -1 ? -1 : _slice_quota_in * _slice_count / (1024L * 1024L),
            _slice_quota_out == -1 ? -1 : _slice_quota_out * _slice_count / (1024L * 1024L),
            _max_pending_buffer_size / (1024L * 1024L),
            _keep_alive_ticks == -1 ? -1 : _keep_alive_ticks / _ticks_per_second);
#endif
}

RpcClientImpl::~RpcClientImpl()
{
    SOFA_PBRPC_FUNCTION_TRACE;
    Stop();
}

void RpcClientImpl::Start()
{
    ScopedLocker<MutexLock> _(_start_stop_lock);
    if (_is_running) return;

    _flow_controller.reset(new FlowController(
                _slice_quota_in == -1, _slice_quota_in,
                _slice_quota_out == -1, _slice_quota_out));

    _maintain_thread_group.reset(new ThreadGroupImpl(
                2, "sofa_pbrpc_client_maintain_thread_group"));
    _maintain_thread_group->start();

    _callback_thread_group.reset(new ThreadGroupImpl(
                _options.callback_thread_num, "sofa_pbrpc_client_callback_thread_group"));
    _callback_thread_group->start();

    _work_thread_group.reset(new ThreadGroupImpl(
                _options.work_thread_num, "sofa_pbrpc_client_work_thread_group"));
    _work_thread_group->start();

    _timer_worker.reset(new TimerWorker(_maintain_thread_group->io_service()));
    _timer_worker->set_time_duration(time_duration_milliseconds(MAINTAIN_INTERVAL_IN_MS)); 
    _timer_worker->set_work_routine(boost::bind(
                &RpcClientImpl::TimerMaintain, shared_from_this(), _1));
    _timer_worker->start();

    _timeout_manager.reset(new RpcTimeoutManager(_maintain_thread_group->io_service()));
    _timeout_manager->start();

    _is_running = true;

#if defined( LOG )
    LOG(INFO) << "Start(): rpc client started";
#else
    SLOG(INFO, "Start(): rpc client started");
#endif
}

void RpcClientImpl::Stop()
{
    ScopedLocker<MutexLock> _(_start_stop_lock);
    if (!_is_running) return;
    _is_running = false;

    _timeout_manager->stop();
    _timer_worker->stop();
    StopStreams();
    _work_thread_group->stop();

    _timeout_manager.reset();
    _timer_worker.reset();
    ClearStreams();
    _callback_thread_group->stop();
    _maintain_thread_group->stop();

    _work_thread_group.reset();
    _callback_thread_group.reset();
    _maintain_thread_group.reset();
    _flow_controller.reset();

#if defined( LOG )
    LOG(INFO) << "Stop(): rpc client stopped";
#else
    SLOG(INFO, "Stop(): rpc client stopped");
#endif
}

RpcClientOptions RpcClientImpl::GetOptions()
{
    return _options;
}

void RpcClientImpl::ResetOptions(const RpcClientOptions& options)
{
    int64 old_slice_quota_in = _slice_quota_in;
    int64 old_slice_quota_out = _slice_quota_out;
    int64 old_max_pending_buffer_size = _max_pending_buffer_size;
    int64 old_keep_alive_ticks = _keep_alive_ticks;

    _options.max_throughput_in = options.max_throughput_in;
    _options.max_throughput_out = options.max_throughput_out;
    _options.max_pending_buffer_size = options.max_pending_buffer_size;
    _options.keep_alive_time = options.keep_alive_time;

    _slice_quota_in = _options.max_throughput_in == -1 ?
        -1 : std::max(0L, _options.max_throughput_in * 1024L * 1024L) / _slice_count;
    _slice_quota_out = _options.max_throughput_out == -1 ?
        -1 : std::max(0L, _options.max_throughput_out * 1024L * 1024L) / _slice_count;
    _max_pending_buffer_size =
        std::max(0L, options.max_pending_buffer_size * 1024L * 1024L);
    _keep_alive_ticks = _options.keep_alive_time == -1 ?
        -1 : std::max(1, _options.keep_alive_time) * _ticks_per_second;

    if (_max_pending_buffer_size != old_max_pending_buffer_size)
    {
<<<<<<< HEAD
        StreamHashSlot* slot = _slot_head;
        while (slot != NULL)
=======
        ScopedLocker<FastLock> _(_stream_map_lock);
        for (StreamMap::iterator it = _stream_map.begin(); it != _stream_map.end(); ++it)
>>>>>>> e8ec4539b66dbcd263263a042ab0f2d5e52665ae
        {
            ScopedLocker<FastLock> _(slot->lock);
            for (StreamMap::iterator stream_map_it = slot->streams.begin();
                    stream_map_it != slot->streams.end(); ++stream_map_it)
            {
                std::set<RpcClientStreamPtr>& stream_set = stream_map_it->second;
                for (std::set<RpcClientStreamPtr>::iterator stream_it = stream_set.begin();
                        stream_it != stream_set.end(); ++stream_it)
                {
                    (*stream_it)->set_max_pending_buffer_size(_max_pending_buffer_size);
                }
            }
            slot = slot->Next();
        }
    }

#if defined( LOG )
    LOG(INFO) << "ResetOptions(): quota_in="
              << (_slice_quota_in == -1 ? -1 : _slice_quota_in * _slice_count / (1024L * 1024L))
              << "MB/s(old "
              << (old_slice_quota_in == -1 ? -1 : old_slice_quota_in * _slice_count / (1024L * 1024L))
              << "MB/s), quota_out="
              << (_slice_quota_out == -1 ? -1 : _slice_quota_out * _slice_count / (1024L * 1024L))
              << "MB/s(old "
              << (old_slice_quota_out == -1 ? -1 : old_slice_quota_out * _slice_count / (1024L * 1024L))
              << "MB/s), max_pending_buffer_size="
              << (_max_pending_buffer_size / (1024L * 1024L))
              << "MB(old "
              << (old_max_pending_buffer_size / (1024L * 1024L))
              << "MB), keep_alive_time="
              << (_keep_alive_ticks == -1 ? -1 : _keep_alive_ticks / _ticks_per_second)
              << "seconds(old "
              << (old_keep_alive_ticks == -1 ? -1 : old_keep_alive_ticks / _ticks_per_second)
              << "seconds)";
#else
    SLOG(INFO, "ResetOptions(): quota_in=%lldMB/s(old %lldMB/s)"
            ", quota_out=%lldMB/s(old %lldMB/s)"
            ", max_pending_buffer_size=%lldMB(old %lldMB)",
            ", keep_alive_time=%lldseconds(old %lldseconds)",
            _slice_quota_in == -1 ? -1 : _slice_quota_in * _slice_count / (1024L * 1024L),
            old_slice_quota_in == -1 ? -1 : old_slice_quota_in * _slice_count / (1024L * 1024L),
            _slice_quota_out == -1 ? -1 : _slice_quota_out * _slice_count / (1024L * 1024L),
            old_slice_quota_out == -1 ? -1 : old_slice_quota_out * _slice_count / (1024L * 1024L),
            _max_pending_buffer_size / (1024L * 1024L),
            old_max_pending_buffer_size / (1024L * 1024L),
            _keep_alive_ticks == -1 ? -1 : _keep_alive_ticks / _ticks_per_second,
            old_keep_alive_ticks == -1 ? -1 : old_keep_alive_ticks / _ticks_per_second);
#endif
}

int RpcClientImpl::ConnectionCount()
{
    int count = 0;
    StreamHashSlot* slot = _slot_head;
    while (slot != NULL)
    {
        ScopedLocker<FastLock> _(slot->lock);
        for (StreamMap::iterator stream_map_it = slot->streams.begin();
                stream_map_it != slot->streams.end(); ++stream_map_it)
        {
            std::set<RpcClientStreamPtr>& stream_set = stream_map_it->second;
            count += stream_set.size();
        }
        slot = slot->Next();
    }
    return count;
}

// Rpc call method to remote endpoint.
//
// The call can be done in following cases:
// * send failed
// * timeouted
// * received response
void RpcClientImpl::CallMethod(const google::protobuf::Message* request,
        google::protobuf::Message* response,
        const RpcControllerImplPtr& cntl)
{
    if (!_is_running)
    {
#if defined( LOG )
        LOG(ERROR) << "CallMethod(): client not in running, ignore";
#else
        SLOG(ERROR, "CallMethod(): client not in running, ignore");
#endif
        cntl->Done(RPC_ERROR_NOT_IN_RUNNING, "client not in running, should start it first");
        return;
    }

    // get the stream
    RpcClientStreamPtr stream = FindOrCreateStream(cntl->RemoteEndpoint());
    if (!stream)
    {
#if defined( LOG )
        LOG(ERROR) << "CallMethod(): create socket stream failed: "
                   << RpcEndpointToString(cntl->RemoteEndpoint());
#else
        SLOG(ERROR, "CallMethod(): create socket stream failed: %s",
                RpcEndpointToString(cntl->RemoteEndpoint()).c_str());
#endif
        cntl->Done(RPC_ERROR_CREATE_STREAM, "create stream failed, maybe exceed connection limit");
        return;
    }
    cntl->SetRpcClientStream(stream);

    // check the pending buffer full
    if (stream->pending_buffer_size() > stream->max_pending_buffer_size())
    {
#if defined( LOG )
#else
        SLOG(DEBUG, "CallMethod(): pending buffer full: %s",
                RpcEndpointToString(cntl->RemoteEndpoint()).c_str());
#endif
        cntl->Done(RPC_ERROR_SEND_BUFFER_FULL, "pending buffer full");
        return;
    }

    // generate sequence id
    cntl->SetSequenceId(GenerateSequenceId());

    // prepare request buffer
    RpcMeta meta;
    meta.set_type(RpcMeta::REQUEST);
    meta.set_sequence_id(cntl->SequenceId());
    meta.set_method(cntl->MethodId());
    int64 timeout = cntl->Timeout();
    if (timeout > 0)
    {
        meta.set_server_timeout(timeout);
    }
    meta.set_compress_type(cntl->RequestCompressType());
    meta.set_expected_response_compress_type(cntl->ResponseCompressType());

    RpcMessageHeader header;
    int header_size = sizeof(header);
    WriteBuffer write_buffer;
    int64 header_pos = write_buffer.Reserve(header_size);
    if (header_pos < 0)
    {
#if defined( LOG )
        LOG(ERROR) << "CallMethod(): " << RpcEndpointToString(cntl->RemoteEndpoint())
                   << ": reserve rpc message header failed";
#else
        SLOG(ERROR, "CallMethod(): %s: reserve rpc message header failed",
                RpcEndpointToString(cntl->RemoteEndpoint()).c_str());
#endif
        cntl->Done(RPC_ERROR_SERIALIZE_REQUEST, "reserve rpc message header failed");
        return;
    }
    if (!meta.SerializeToZeroCopyStream(&write_buffer))
    {
#if defined( LOG )
        LOG(ERROR) << "CallMethod(): " << RpcEndpointToString(cntl->RemoteEndpoint())
                   << ": serialize rpc meta failed";
#else
        SLOG(ERROR, "CallMethod(): %s: serialize rpc meta failed",
                RpcEndpointToString(cntl->RemoteEndpoint()).c_str());
#endif
        cntl->Done(RPC_ERROR_SERIALIZE_REQUEST, "serialize rpc meta failed");
        return;
    }
    header.meta_size = static_cast<int>(write_buffer.ByteCount() - header_pos - header_size);
    bool serialize_request_return = false;
    if (meta.compress_type() == CompressTypeNone)
    {
        serialize_request_return = request->SerializeToZeroCopyStream(&write_buffer);
    }
    else
    {
        ::sofa::pbrpc::scoped_ptr<AbstractCompressedOutputStream> os(
                get_compressed_output_stream(&write_buffer, meta.compress_type()));
        serialize_request_return = request->SerializeToZeroCopyStream(os.get());
        os->Flush();
    }
    if (!serialize_request_return)
    {
#if defined( LOG )
        LOG(ERROR) << "CallMethod(): " << RpcEndpointToString(cntl->RemoteEndpoint())
                   << ": serialize request message failed";
#else
        SLOG(ERROR, "CallMethod(): %s: serialize request message failed",
                RpcEndpointToString(cntl->RemoteEndpoint()).c_str());
#endif
        cntl->Done(RPC_ERROR_SERIALIZE_REQUEST, "serialize request message failed");
        return;
    }
    header.data_size = write_buffer.ByteCount() - header_pos - header_size - header.meta_size;
    header.message_size = header.meta_size + header.data_size;
    write_buffer.SetData(header_pos, reinterpret_cast<const char*>(&header), header_size);

    ReadBufferPtr read_buffer(new ReadBuffer());
    write_buffer.SwapOut(read_buffer.get());
    cntl->SetRequestBuffer(read_buffer);

    // push callback
    cntl->PushDoneCallback(boost::bind(&RpcClientImpl::DoneCallback, shared_from_this(), response, _1));

    // add to timeout manager if need
    if (timeout > 0)
    {
        if (!_timeout_manager->add(cntl))
        {
#if defined( LOG )
            LOG(ERROR) << "CallMethod(): " << RpcEndpointToString(cntl->RemoteEndpoint())
                       << ": add to timeout manager failed: timeout=" << timeout << "ms";
#else
            SLOG(ERROR, "CallMethod(): %s: add to timeout manager failed: timeout=%lldms",
                    RpcEndpointToString(cntl->RemoteEndpoint()).c_str(), timeout);
#endif
            cntl->Done(RPC_ERROR_REQUEST_TIMEOUT, "add to timeout manager failed, maybe too short timeout");
            return;
        }
    }

    // call method
    stream->call_method(cntl);
}

const ThreadGroupImplPtr& RpcClientImpl::GetCallbackThreadGroup() const
{
    return _callback_thread_group;
}

bool RpcClientImpl::ResolveAddress(const std::string& address,
        RpcEndpoint* endpoint)
{
    return sofa::pbrpc::ResolveAddress(_work_thread_group->io_service(), address, endpoint);
}

RpcClientStreamPtr RpcClientImpl::FindOrCreateStream(const RpcEndpoint& remote_endpoint)
{
    RpcClientStreamPtr stream;
    bool is_find = false;
    bool create = false;
    uint64 slot_index = StreamSlotIndex(remote_endpoint);
    if (_stream_slots[slot_index] == NULL)
    {
        _stream_slots[slot_index] = new StreamHashSlot(_slot_head);
        _slot_head = _stream_slots[slot_index];
    }
    StreamHashSlot* slot = _stream_slots[slot_index];
    {
        ScopedLocker<FastLock> _(slot->lock);
        std::set<RpcClientStreamPtr>& stream_set = slot->streams[remote_endpoint];
        switch (_connection_type)
        {
            case SINGLE_CONNECTION:
            {
                if (!stream_set.empty())
                {
                    if (!(*stream_set.begin())->is_closed())
                    {
                        stream = *(stream_set.begin());
                        is_find = true;
                    }
                    else
                    {
                        stream_set.clear();
                    }
                }
                break;
            }
            case MULTI_CONNECTION:
            {
                if (stream_set.size() == static_cast<size_t>(_multi_connection_count))
                {
                    int64 min_pending_count = kint64max;
                    std::set<RpcClientStreamPtr>::iterator min_iterator = stream_set.end();
                    for (std::set<RpcClientStreamPtr>::iterator stream_it = stream_set.begin(); 
                            stream_it != stream_set.end();)
                    {
                        if ((*stream_it)->is_closed())
                        {
                            stream_set.erase(stream_it++);
                            continue;
                        }
                        if ((*stream_it)->pending_message_count() < min_pending_count)
                        {
                            min_pending_count = (*stream_it)->pending_message_count();
                            min_iterator = stream_it;
                            is_find = true;
                        }
                        ++stream_it;
                    }
                    if (is_find)
                    {
                        stream = (*min_iterator);
                    }
                }
                break;
            }
            default:
            {
                SCHECK(false);
                break;
            }
        }
        if (!is_find)
        {
            stream.reset(new RpcClientStream(_work_thread_group->io_service(), remote_endpoint));
            stream->set_flow_controller(_flow_controller);
            stream->set_max_pending_buffer_size(_max_pending_buffer_size);
            stream->reset_ticks((ptime_now() - _epoch_time).ticks(), true);
            stream->set_connect_timeout(_options.connect_timeout);
            stream->set_closed_stream_callback(
                    boost::bind(&RpcClientImpl::OnClosed, shared_from_this(), _1));
            stream_set.insert(stream);
            create = true;
        }
    }
    if (create)
    {
        stream->async_connect();
    }
    return stream;
}

void RpcClientImpl::OnClosed(const RpcClientStreamPtr& stream)
{
    if (!_is_running)
        return;

    uint64 slot_index = StreamSlotIndex(stream->remote_endpoint());
    StreamHashSlot* slot = _stream_slots[slot_index];
    if (slot != NULL)
    {
        ScopedLocker<FastLock> _(slot->lock);
        StreamMap::iterator find = slot->streams.find(stream->remote_endpoint());
        if (find != slot->streams.end())
        {
            find->second.erase(stream);
        }
    }
}

void RpcClientImpl::StopStreams()
{
    StreamHashSlot* slot = _slot_head;
    while (slot != NULL)
    {
        ScopedLocker<FastLock> _(slot->lock);
        for (StreamMap::iterator stream_map_it = slot->streams.begin();
                stream_map_it != slot->streams.end(); ++stream_map_it)
        {
            std::set<RpcClientStreamPtr>& stream_set = stream_map_it->second;
            for (std::set<RpcClientStreamPtr>::iterator stream_it = stream_set.begin();
                    stream_it != stream_set.end(); ++stream_it)
            {
                (*stream_it)->close("client stopped");
            }
        }
        slot = slot->Next();
    }
}

void RpcClientImpl::ClearStreams()
{
    StreamHashSlot* slot = _slot_head;
    while (slot != NULL)
    {
        ScopedLocker<FastLock> _(slot->lock);
        for (StreamMap::iterator stream_map_it = slot->streams.begin(); 
                stream_map_it != slot->streams.end(); ++stream_map_it)
        {
            stream_map_it->second.clear();
        }
        slot->streams.clear();
        StreamHashSlot* slot_backup = slot;
        slot = slot->Next();
        delete slot_backup;
    }
}

void RpcClientImpl::DoneCallback(google::protobuf::Message* response,
        const RpcControllerImplPtr& cntl)
{
    // erase from RpcTimeoutManager
    _timeout_manager->erase(cntl->TimeoutId());

    // deserialize response
    if (!cntl->Failed())
    {
        SCHECK(response != NULL);
        SCHECK(cntl->ResponseBuffer());
        ReadBufferPtr buffer = cntl->ResponseBuffer();
        CompressType compress_type = cntl->ResponseCompressType();
        bool parse_response_return = false;
        if (compress_type == CompressTypeNone)
        {
            parse_response_return = response->ParseFromZeroCopyStream(buffer.get());
        }
        else
        {
            ::sofa::pbrpc::scoped_ptr<AbstractCompressedInputStream> is(
                    get_compressed_input_stream(buffer.get(), compress_type));
            parse_response_return = response->ParseFromZeroCopyStream(is.get());
        }
        if (!parse_response_return)
        {
#if defined( LOG )
            LOG(ERROR) << "DoneCallback(): " << RpcEndpointToString(cntl->RemoteEndpoint())
                       << ": parse response message pb failed";
#else
            SLOG(ERROR, "DoneCallback(): %s: parse response message pb failed",
                    RpcEndpointToString(cntl->RemoteEndpoint()).c_str());
#endif
            cntl->SetFailed(RPC_ERROR_PARSE_RESPONSE_MESSAGE, "parse response message pb failed");
        }
    }
}

void RpcClientImpl::TimerMaintain(const PTime& now)
{
    SOFA_PBRPC_FUNCTION_TRACE;

    int64 now_ticks = (now - _epoch_time).ticks();

    // checks which need iterator streams
    if (_keep_alive_ticks != -1 || _slice_quota_in != -1 || _slice_quota_out != -1)
    {
        // check keep alive time
        if (_keep_alive_ticks != -1)
        {
            StreamHashSlot* slot = _slot_head;
            while (slot != NULL)
            {
                ScopedLocker<FastLock> _(slot->lock);
                for (StreamMap::iterator stream_map_it = slot->streams.begin();
                        stream_map_it != slot->streams.end(); ++stream_map_it)
                {
                    std::set<RpcClientStreamPtr>& stream_set = stream_map_it->second;
                    for (std::set<RpcClientStreamPtr>::iterator stream_it = stream_set.begin();
                            stream_it != stream_set.end(); ++stream_it)
                    {
                        const RpcClientStreamPtr& stream = (*stream_it);
                        if (stream->is_closed())
                        {
                            continue;
                        }
                        if (now_ticks - stream->last_rw_ticks() >= _keep_alive_ticks
                                && stream->pending_process_count() == 0)
                        {
                            stream->close("keep alive timeout: "
                                          + boost::lexical_cast<std::string>(_options.keep_alive_time)
                                          + " seconds");
                        }
                        else
                        {
                            stream->reset_ticks(now_ticks, false);
                        }
                    }
                }
                slot = slot->Next();
            }
        }

        // flow control in
        if (_slice_quota_in != -1)
        {
            // recharge quota pool
            _flow_controller->recharge_read_quota(_slice_quota_in);

            // collect streams need to trigger
            std::vector<FlowControlItem> trigger_list;
            trigger_list.reserve(STREAM_HASH_SLOT_COUNT * _multi_connection_count);

            StreamHashSlot* slot = _slot_head;
            while (slot != NULL)
            {
                ScopedLocker<FastLock> _(slot->lock);
                for (StreamMap::iterator stream_map_it = slot->streams.begin();
                        stream_map_it != slot->streams.end(); ++stream_map_it)
                {
                    std::set<RpcClientStreamPtr>& stream_set = stream_map_it->second;
                    for (std::set<RpcClientStreamPtr>::iterator stream_it = stream_set.begin();
                            stream_it != stream_set.end(); ++stream_it)
                    {
                        const RpcClientStreamPtr& stream = (*stream_it);
                        if (stream->is_closed())
                        {
                            continue;
                        }
                        int token = stream->read_quota_token();
                        if (token <= 0)
                        {
                            // only need trigger streams whose token <= 0
                            trigger_list.push_back(FlowControlItem(token, stream.get()));
                        }
                    }
                }
                slot = slot->Next();
            }

            // sort by token: token closer to zero, earlier to trigger
            std::sort(trigger_list.begin(), trigger_list.end());

            // trigger in order
            for (std::vector<FlowControlItem>::iterator t_it = trigger_list.begin();
                    t_it != trigger_list.end(); ++t_it)
            {
                t_it->stream->trigger_receive();
            }
        }

        // flow control out
        if (_slice_quota_out != -1)
        {
            // recharge quota pool
            _flow_controller->recharge_write_quota(_slice_quota_out);

            // collect streams need to trigger
            std::vector<FlowControlItem> trigger_list;
            trigger_list.reserve(STREAM_HASH_SLOT_COUNT * _multi_connection_count);

            StreamHashSlot* slot = _slot_head;
            while (slot != NULL)
            {
                ScopedLocker<FastLock> _(slot->lock);
                for (StreamMap::iterator stream_map_it = slot->streams.begin();
                        stream_map_it != slot->streams.end(); ++stream_map_it)
                {
                    std::set<RpcClientStreamPtr>& stream_set = stream_map_it->second;
                    for (std::set<RpcClientStreamPtr>::iterator stream_it = stream_set.begin();
                            stream_it != stream_set.end(); ++stream_it)
                    {
                        const RpcClientStreamPtr& stream = (*stream_it);
                        if (stream->is_closed())
                        {
                            continue;
                        }
                        int token = stream->read_quota_token();
                        if (token <= 0)
                        {
                            // only need trigger streams whose token <= 0
                            trigger_list.push_back(FlowControlItem(token, stream.get()));
                        }
                    }
                }
                slot = slot->Next();
            }

            // sort by token: token closer to zero, earlier to trigger
            std::sort(trigger_list.begin(), trigger_list.end());

            // trigger in order
            for (std::vector<FlowControlItem>::iterator t_it = trigger_list.begin();
                    t_it != trigger_list.end(); ++t_it)
            {
                t_it->stream->trigger_send();
            }
        }
    }

    _last_maintain_ticks = now_ticks;
}

uint64 RpcClientImpl::GenerateSequenceId()
{
    return static_cast<uint64>(++_next_request_id);
}

uint64 RpcClientImpl::StreamSlotIndex(const RpcEndpoint& remote_endpoint)
{
    std::string endpoint_str = RpcEndpointToString(remote_endpoint);
    return murmurhash(endpoint_str.c_str(), endpoint_str.size()) % STREAM_HASH_SLOT_COUNT;
}

} // namespace pbrpc
} // namespace sofa

/* vim: set ts=4 sw=4 sts=4 tw=100 */
