/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/*
 * Note: You can change/add any functions in MP1Node.{h,cpp}
 */

/**
 * Overloaded Constructor of the MP1Node class
 * You can add new members to the class if you think it
 * is necessary for your logic to work
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log, Address *address) {
	for( int i = 0; i < 6; i++ ) {
		NULLADDR[i] = 0;
	}
	this->memberNode = member;
	this->emulNet = emul;
	this->log = log;
	this->par = params;
	this->memberNode->addr = *address;

}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into the queue
 * 				This function is called by a node to receive messages currently waiting for it
 */
int MP1Node::recvLoop() {
    if ( memberNode->bFailed ) {
    	return false;
    }
    else {
    	return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1, &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
	Queue q;
	return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {

    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if( initThisNode(&joinaddr) == -1 ) {
        exit(1);
    }

    if( !introduceSelfToGroup(&joinaddr) ) {
        finishUpThisNode();
        exit(1);
    }

    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {

	int id = *(int*)(&memberNode->addr.addr);
	int port = *(short*)(&memberNode->addr.addr[4]);

	memberNode->bFailed = false;
	memberNode->inited = true;
	memberNode->inGroup = false;

    // node is up!
	memberNode->nnb = 0;
	memberNode->heartbeat = 0;
	memberNode->pingCounter = TFAIL;
	memberNode->timeOutCounter = -1;
    clearMemberListTable(memberNode);

    // Position in the membership list is set
    memberNode->myPos = addEntryToMemberList(id, port, memberNode->heartbeat);

    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {

	MessageHdr *msg;

    #ifdef DEBUGLOG
    static char logtext[1024];
    #endif

    if ( 0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr), sizeof(memberNode->addr.addr))) {
        memberNode->inGroup = true;
    } else {
        size_t msgsize = sizeof(MessageHdr) + sizeof(joinaddr->addr) + sizeof(long) + 1;
        msg = (MessageHdr *) malloc(msgsize * sizeof(char));

        msg->msgType = JOINREQ;
        memcpy((char *)(msg+1), &memberNode->addr.addr, sizeof(memberNode->addr.addr));
        memcpy((char *)(msg+1) + 1 + sizeof(memberNode->addr.addr), &memberNode->heartbeat, sizeof(long));

        #ifdef DEBUGLOG
        log->LOG(&memberNode->addr, logtext);
        #endif

        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    return 1;
}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode(){
    clearMemberListTable(memberNode);
    return SUCCESS;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform membership protocol duties
 */
void MP1Node::nodeLoop() {

    if (memberNode->bFailed) {
    	return;
    }

    checkMessages();

    if( !memberNode->inGroup ) {
    	return;
    }

    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message handler
 */
void MP1Node::checkMessages() {

    void *ptr;
    int size;

    while ( !memberNode->mp1q.empty() ) {
    	ptr = memberNode->mp1q.front().elt;
    	size = memberNode->mp1q.front().size;
    	memberNode->mp1q.pop();
    	recvCallBack((void *)memberNode, (char *)ptr, size);
    }

    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(void *env, char *data, int size ) {

    MessageHdr *msg = (MessageHdr *) data;
    char *packetData = (char *) (msg + 1);

    if ( (unsigned)size < sizeof(MessageHdr)) {
        return false;
    }

    // Invalid Message Types
    if (msg->msgType < 0 || msg->msgType > DUMMYLASTMSGTYPE) {
        return true;
    }

    if (!memberNode->inGroup && msg->msgType != JOINREP) {
        return true;
    }

    switch(msg->msgType) {

        case JOINREQ:
            processJoinReq(env, packetData, size - sizeof(MessageHdr));
            break;

        case JOINREP:
            processJoinRep(env, packetData, size - sizeof(MessageHdr));
            break;

        case UPDATEREQ:
            processUpdateReq(env, packetData, size - sizeof(MessageHdr));
            break;

        case UPDATEREP:
            processUpdateRep(env, packetData, size - sizeof(MessageHdr));
            break;

        default:
            break;
    }

    return true;
}

/**
* FUNCTION NAME: processJoinReq
*
* DESCRIPTION: This function is run by the coordinator to process JOIN requests
*/
void MP1Node::processJoinReq(void *env, char *data, int size) {

    Member *node = (Member*)env;
    Address newaddr;
    long heartbeat;
    memcpy(newaddr.addr, data, sizeof(newaddr.addr));
    memcpy(&heartbeat, data+1+sizeof(newaddr.addr), sizeof(long));
    int id = *(int*)(&newaddr.addr);
    short port = *(short*)(&newaddr.addr[4]);
    char *table = generateUpdateTable(node);
    size_t sz = sizeof(MessageHdr) + 1 + strlen(table);
    MessageHdr *msg = (MessageHdr *) malloc(sz * sizeof(char));

    // Message is set to JOINREP
    msg->msgType = JOINREP;

    char * ptr = (char *) (msg+1);
    memcpy(ptr, table, strlen(table)+1);

    emulNet->ENsend(&node->addr, &newaddr, (char *)msg, (int)sz);

    // Garbage Collection
    free(msg);
    free(table);

    vector<MemberListEntry>::iterator iter;
    iter = scanMemberListForIdAndPort(id, port);

    if ( iter == memberNode->memberList.end() ) {
        addEntryToMemberList(id, port, heartbeat);
    }

    return;
}

/**
* FUNCTION NAME: procesJoinRep
*
* DESCRIPTION: This function is the message handler for JOINREP. The list will be present in the message.
* 				It needs to add the list to its own table. Data parameter contains the entire table
*/
void MP1Node::processJoinRep(void *env, char *data, int size) {
#ifdef DEBUGLOG
    static char s[1024];
    char *s1 = s;
    s1 += sprintf(s1, "Received neighbor list:");
#endif
    processUpdateTable(data);
    memberNode->inGroup = true;
    return;
}

/**
* FUNCTION NAME: processUpdateReq
*
* DESCRIPTION: This function is the message handler for UPDATEREQ
* note: we dont need updatereq here; each node gossip its table to other nodes periodically
* and the gossip action is done inside the nodeloopops
*/
void MP1Node::processUpdateReq(void *env, char *data, int size) {
    return;
}

/**
* FUNCTION NAME: processUpdateRep
*
* DESCRIPTION: This function is the message handler for UPDATEREP
* when each node receives a table, it needs to call the deserializeAndUpdate function to update its own table
* data: the real message (the table without the type )
*/
void MP1Node::processUpdateRep(void *env, char *data, int size) {
    processUpdateTable(data);
    return;
}

/**
* FUNCTION NAME: nodeLoopOps
*
* DESCRIPTION: process node
*/
void MP1Node::nodeLoopOps() {

    deleteTimedOutNodes();

    memberNode->heartbeat++;

    int id = *(int*)(&memberNode->addr.addr);
    for ( vector<MemberListEntry>::iterator it = memberNode->memberList.begin(); it != memberNode->memberList.end(); it++ ) {
        // This is your own node
        if( id == (*(it)).getid() ) {
            (*(it)).setheartbeat(memberNode->heartbeat);
        }
    }

    if ( 0 == --(memberNode->pingCounter) ) {
        char *table = generateUpdateTable(memberNode);

        size_t sz = sizeof(MessageHdr) + 1 + strlen(table);
        MessageHdr *msg = (MessageHdr *)malloc(sz * sizeof(char));
        msg->msgType = UPDATEREP;
        char *ptr = (char *)(msg + 1);
        memcpy(ptr, table, strlen(table)+1);

        for (vector<MemberListEntry>::iterator iter = memberNode->memberList.begin(); iter != memberNode->memberList.end(); ++iter ) {
            Address addr;
            assembleAddress(&addr, (*(iter)).getid(), (*(iter)).getport());
            if ( memcmp(addr.addr, memberNode->addr.addr, sizeof(addr.addr)) != 0 ) {
                emulNet->ENsend(&memberNode->addr, &addr, (char *)msg, sz);
            }
        }

        free(msg);
        free(table);

        memberNode->pingCounter = TFAIL;
    }
    return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
	return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {

    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

// Clears the MembershipTable
void MP1Node::clearMemberListTable(Member *memberNode) {
    memberNode->memberList.clear();
}

vector<MemberListEntry>::iterator MP1Node::addEntryToMemberList(int id, short port, long heartbeat) {

    vector<MemberListEntry>::iterator iter;

    if( id > par->MAX_NNB ) {
        return iter;
    }

    MemberListEntry newEntry(id, port, heartbeat, par->getcurrtime());
    memberNode->memberList.emplace_back(newEntry);
    memberNode->nnb++;

    Address addr;
    assembleAddress(&addr, id, port);
    log->logNodeAdd(&memberNode->addr, &addr);

    iter = memberNode->memberList.end();
    return --iter;
}

void MP1Node::deleteTimedOutNodes() {

    if ( memberNode->memberList.empty() ) {
        return;
    }

    int id = *(int*)(&memberNode->addr.addr);

    for ( int i = 0; i < memberNode->memberList.size(); i++ ) {
        if ( id != memberNode->memberList.at(i).id && ( par->getcurrtime() - (memberNode->memberList.at(i).timestamp) ) > TREMOVE ) {
            Address addr_to_delete;
            assembleAddress(&addr_to_delete, memberNode->memberList.at(i).id, memberNode->memberList.at(i).port);
            memberNode->memberList.erase(memberNode->memberList.begin() + i);
            memberNode->nnb--;
            log->logNodeRemove(&memberNode->addr, &addr_to_delete);
        }
    }
}

char* MP1Node::generateUpdateTable(Member *node)
{
    char *result = NULL;
    for ( unsigned int i = 0; i < memberNode->memberList.size(); i++ ) {

        char * entry = NULL;

        asprintf(
            &entry,
            "%d:%hi~%ld~%ld",
            memberNode->memberList.at(i).getid(),
            memberNode->memberList.at(i).getport(),
            memberNode->memberList.at(i).getheartbeat(),
            memberNode->memberList.at(i).gettimestamp()
        );

        // Append to result
        if(!result) {
            asprintf(&result, "%s|", entry);
        } else {
            asprintf(&result, "%s%s|", result, entry);
        }

        // Garbage Collection
        if(entry) {
            free(entry);
        }

    }
    return result;
}

char* MP1Node::processUpdateTable(const char *msg){

    char *buffer = NULL;
    asprintf(&buffer, "%s", msg);

    char * updateTable;
    updateTable = strtok (buffer,"|");

    while (updateTable != NULL) {
        char *row = NULL;
        asprintf(&row, "%s", updateTable);
        int id;
        short port;
        long heartbeat;
        long timestamp;

        sscanf(row,"%d:%hi~%ld~%ld", &id, &port, &heartbeat, &timestamp);
        vector<MemberListEntry>::iterator iter = scanMemberListForIdAndPort(id, port);

        if( iter != memberNode->memberList.end() ) {

            if( (*(iter)).getheartbeat() < heartbeat ) {
                (*(iter)).setheartbeat(heartbeat);
                (*(iter)).settimestamp(par->getcurrtime());
            }

        } else {
            addEntryToMemberList(id, port, heartbeat);
        }

        updateTable = strtok (NULL, "|");

    }
    return NULL;
}

void MP1Node::assembleAddress(Address *addr, int id, short port) {
    // Adds id as first part
    memcpy(&addr->addr[0], &id,  sizeof(int));

    // Adds port as last part
    memcpy(&addr->addr[4], &port, sizeof(short));
}

vector<MemberListEntry>::iterator MP1Node::scanMemberListForIdAndPort(int id, short port) {

    vector<MemberListEntry>::iterator iter;

    for ( iter = memberNode->memberList.begin(); iter != memberNode->memberList.end(); ++iter ) {
        if( (*(iter)).getid() == id ) {
            return iter;
        }
    }

    return iter;
}

/**
 * FUNCTION NAME: printAddress
 *
 * DESCRIPTION: Print the Address
 */
void MP1Node::printAddress(Address *addr)
{
    printf("%d.%d.%d.%d:%d \n",  addr->addr[0],addr->addr[1],addr->addr[2],
                                                       addr->addr[3], *(short*)&addr->addr[4]) ;    
}
//EOF