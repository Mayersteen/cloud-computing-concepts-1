/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Header file of MP1Node class.
 **********************************/

#ifndef _MP1NODE_H_
#define _MP1NODE_H_

#include "stdincludes.h"
#include "Log.h"
#include "Params.h"
#include "Member.h"
#include "EmulNet.h"
#include "Queue.h"

/**
 * Macros
 */
#define TREMOVE 20
#define TFAIL 5

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Message Types
 */
enum MsgTypes{
    JOINREQ,
    JOINREP,
	UPDATEREQ,
	UPDATEREP,
    DUMMYLASTMSGTYPE
};

/**
 * STRUCT NAME: MessageHdr
 *
 * DESCRIPTION: Header and content of a message
 */
typedef struct MessageHdr {
	enum MsgTypes msgType;
}MessageHdr;

/**
 * CLASS NAME: MP1Node
 *
 * DESCRIPTION: Class implementing Membership protocol functionalities for failure detection
 */
class MP1Node {
private:
	EmulNet *emulNet;
	Log *log;
	Params *par;
	Member *memberNode;
	char NULLADDR[6];

public:
	MP1Node(Member *, Params *, EmulNet *, Log *, Address *);
	Member * getMemberNode() {
		return memberNode;
	}
	int recvLoop();
	static int enqueueWrapper(void *env, char *buff, int size);
	void nodeStart(char *servaddrstr, short serverport);
	int initThisNode(Address *joinaddr);
	int introduceSelfToGroup(Address *joinAddress);
	int finishUpThisNode();
	void nodeLoop();
	void checkMessages();
	bool recvCallBack(void *env, char *data, int size);

	// Update and Join functions
	void processUpdateRep(void *env, char *data, int size);
	void processUpdateReq(void *env, char *data, int size);
	void processJoinRep(void *env, char *data, int size);
	void processJoinReq(void *env, char *data, int size);

	void nodeLoopOps();

	int isNullAddress(Address *addr);
	Address getJoinAddress();
	void printAddress(Address *addr);

	void deleteTimedOutNodes();
	void clearMemberListTable(Member *memberNode);
	vector<MemberListEntry>::iterator addEntryToMemberList(int id, short port, long heartbeat);
	char*generateUpdateTable(Member *node);
	char*processUpdateTable(const char *msg);
	vector<MemberListEntry>::iterator scanMemberListForIdAndPort(int id, short port);
	void assembleAddress(Address *addr, int id, short port);

	virtual ~MP1Node();
};

#endif /* _MP1NODE_H_ */
