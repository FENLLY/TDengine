/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tq.h"

void tqTmrRspFunc(void* param, void* tmrId) {
  STqHandle* pHandle = (STqHandle*)param;
  atomic_store_8(&pHandle->pushHandle.tmrStopped, 1);
}

int32_t tqExecFromInputQ(STQ* pTq, STqHandle* pHandle) {
  // 1. guard and set status executing
  // 2. check processedVer
  // 2.1. if not missed, get msg from queue
  // 2.2. if missed, scan wal
  //
  // 3. exec, after each success, update processed ver
  // first run
  // set exec status closing
  // second run
  // set exec status idle
  //
  // 4. if get result
  // 4.1 set exec input status blocked and exec status idle
  // 4.2 rpc send
  // 4.3 clear rpc info
  return 0;
}

int32_t tqOpenPushHandle(STQ* pTq, STqHandle* pHandle) {
  memset(&pHandle->pushHandle, 0, sizeof(STqPushHandle));
  pHandle->pushHandle.inputQ.queue = taosOpenQueue();
  pHandle->pushHandle.inputQ.qall = taosAllocateQall();
  if (pHandle->pushHandle.inputQ.queue == NULL || pHandle->pushHandle.inputQ.qall == NULL) {
    if (pHandle->pushHandle.inputQ.queue) {
      taosCloseQueue(pHandle->pushHandle.inputQ.queue);
    }
    if (pHandle->pushHandle.inputQ.qall) {
      taosFreeQall(pHandle->pushHandle.inputQ.qall);
    }
    return -1;
  }
  return 0;
}

int32_t tqPreparePush(STQ* pTq, STqHandle* pHandle, int64_t reqId, const SRpcHandleInfo* pInfo, int64_t processedVer,
                      int64_t timeout) {
  memcpy(&pHandle->pushHandle.rpcInfo, pInfo, sizeof(SRpcHandleInfo));
  atomic_store_64(&pHandle->pushHandle.reqId, reqId);
  atomic_store_64(&pHandle->pushHandle.processedVer, processedVer);
  atomic_store_8(&pHandle->pushHandle.inputStatus, TASK_INPUT_STATUS__NORMAL);
  atomic_store_8(&pHandle->pushHandle.tmrStopped, 0);
  taosTmrReset(tqTmrRspFunc, (int32_t)timeout, pHandle, tqMgmt.timer, &pHandle->pushHandle.timerId);
  return 0;
}

int32_t tqEnqueue(STqHandle* pHandle, SStreamDataSubmit* pSubmit) {
  int8_t inputStatus = atomic_load_8(&pHandle->pushHandle.inputStatus);
  if (inputStatus == TASK_INPUT_STATUS__NORMAL) {
    SStreamDataSubmit* pSubmitClone = streamSubmitRefClone(pSubmit);
    if (pSubmitClone == NULL) {
      return -1;
    }
    taosWriteQitem(pHandle->pushHandle.inputQ.queue, pSubmitClone);
    return 0;
  }
  return -1;
}

int32_t tqSendExecReq(STQ* pTq, STqHandle* pHandle) {
  //
  return 0;
}

int32_t tqEnqueueAll(STQ* pTq, SSubmitReq* pReq) {
  void*              pIter = NULL;
  SStreamDataSubmit* pSubmit = streamDataSubmitNew(pReq);
  if (pSubmit == NULL) {
    return -1;
  }

  while (1) {
    pIter = taosHashIterate(pTq->handles, pIter);
    if (pIter == NULL) break;
    STqHandle* pHandle = (STqHandle*)pIter;
    if (tqEnqueue(pHandle, pSubmit) < 0) {
      continue;
    }
    int8_t execStatus = atomic_load_8(&pHandle->pushHandle.execStatus);
    if (execStatus == TASK_STATUS__IDLE || execStatus == TASK_STATUS__CLOSING) {
      tqSendExecReq(pTq, pHandle);
    }
  }

  streamDataSubmitRefDec(pSubmit);

  return 0;
}

int32_t tqPushMsgNew(STQ* pTq, void* msg, int32_t msgLen, tmsg_t msgType, int64_t ver, SRpcHandleInfo handleInfo) {
  if (msgType != TDMT_VND_SUBMIT) return 0;
  void*       pIter = NULL;
  STqHandle*  pHandle = NULL;
  SSubmitReq* pReq = (SSubmitReq*)msg;
  int32_t     workerId = 4;
  int64_t     fetchOffset = ver;

  while (1) {
    pIter = taosHashIterate(pTq->pushMgr, pIter);
    if (pIter == NULL) break;
    pHandle = *(STqHandle**)pIter;

    taosWLockLatch(&pHandle->pushHandle.lock);

    SMqDataBlkRsp rsp = {0};
    rsp.reqOffset = pHandle->pushHandle.reqOffset;
    rsp.blockData = taosArrayInit(0, sizeof(void*));
    rsp.blockDataLen = taosArrayInit(0, sizeof(int32_t));

    if (msgType == TDMT_VND_SUBMIT) {
      tqDataExec(pTq, &pHandle->execHandle, pReq, &rsp, workerId);
    } else {
      // TODO
      ASSERT(0);
    }

    if (rsp.blockNum == 0) {
      taosWUnLockLatch(&pHandle->pushHandle.lock);
      continue;
    }

    ASSERT(taosArrayGetSize(rsp.blockData) == rsp.blockNum);
    ASSERT(taosArrayGetSize(rsp.blockDataLen) == rsp.blockNum);

    rsp.rspOffset = fetchOffset;

    int32_t tlen = sizeof(SMqRspHead) + tEncodeSMqDataBlkRsp(NULL, &rsp);
    void*   buf = rpcMallocCont(tlen);
    if (buf == NULL) {
      // todo free
      return -1;
    }

    ((SMqRspHead*)buf)->mqMsgType = TMQ_MSG_TYPE__POLL_RSP;
    ((SMqRspHead*)buf)->epoch = pHandle->pushHandle.epoch;
    ((SMqRspHead*)buf)->consumerId = pHandle->pushHandle.consumerId;

    void* abuf = POINTER_SHIFT(buf, sizeof(SMqRspHead));
    tEncodeSMqDataBlkRsp(&abuf, &rsp);

    SRpcMsg resp = {
        .info = pHandle->pushHandle.rpcInfo,
        .pCont = buf,
        .contLen = tlen,
        .code = 0,
    };
    tmsgSendRsp(&resp);

    memset(&pHandle->pushHandle.rpcInfo, 0, sizeof(SRpcHandleInfo));
    taosWUnLockLatch(&pHandle->pushHandle.lock);

    tqDebug("vg %d offset %ld from consumer %ld (epoch %d) send rsp, block num: %d, reqOffset: %ld, rspOffset: %ld",
            TD_VID(pTq->pVnode), fetchOffset, pHandle->pushHandle.consumerId, pHandle->pushHandle.epoch, rsp.blockNum,
            rsp.reqOffset, rsp.rspOffset);

    // TODO destroy
    taosArrayDestroy(rsp.blockData);
    taosArrayDestroy(rsp.blockDataLen);
  }

  return 0;
}

int tqPushMsg(STQ* pTq, void* msg, int32_t msgLen, tmsg_t msgType, int64_t ver) {
  if (msgType == TDMT_VND_SUBMIT) {
    if (taosHashGetSize(pTq->pStreamTasks) == 0) return 0;

    if (tdUpdateExpireWindow(pTq->pVnode->pSma, msg, ver) != 0) {
      // TODO handle sma error
    }
    void* data = taosMemoryMalloc(msgLen);
    if (data == NULL) {
      return -1;
    }
    memcpy(data, msg, msgLen);

    tqProcessStreamTrigger(pTq, data);
  }

  return 0;
}

