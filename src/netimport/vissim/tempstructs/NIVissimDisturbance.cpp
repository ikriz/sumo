#include <map>
#include <string>
#include <iostream>
#include <cassert>
#include <utils/convert/ToString.h>
#include <utils/geom/GeomHelper.h>
#include <utils/geom/Boundery.h>
#include <netbuild/NBEdge.h>
#include <netbuild/NBNode.h>
#include <netbuild/NBEdgeCont.h>
#include <netbuild/NBNodeCont.h>
#include "NIVissimEdge.h"
#include "NIVissimConnection.h"
#include "NIVissimNodeDef.h"
#include "NIVissimDisturbance.h"
#include "NIVissimNodeParticipatingEdgeVector.h"

using namespace std;

NIVissimDisturbance::DictType NIVissimDisturbance::myDict;
int NIVissimDisturbance::myRunningID = 0;


NIVissimDisturbance::NIVissimDisturbance(int id,
                                         const std::string &name,
                                         const NIVissimExtendedEdgePoint &edge,
                                         const NIVissimExtendedEdgePoint &by,
                                         double timegap, double waygap,
                                         double vmax)
    : myID(id), myNode(-1), myName(name), myEdge(edge), myDisturbance(by),
    myTimeGap(timegap), myWayGap(waygap), myVMax(vmax)
{
}


NIVissimDisturbance::~NIVissimDisturbance()
{
}
/*

bool
NIVissimDisturbance::tryAssignToNodeSingle(int nodeid,
        const NIVissimNodeParticipatingEdgeVector &edges)
{
    for(NIVissimNodeParticipatingEdgeVector::const_iterator i=edges.begin();
            i!=edges.end(); i++) {
        NIVissimNodeParticipatingEdge *edge = *i;
        if( edge->getID()==myEdge.getEdgeID() &&
            edge->positionLiesWithin(myEdge.getPosition()) &&
            edge->getID()==myDisturbance.getEdgeID() &&
            edge->positionLiesWithin(myDisturbance.getPosition())) {
            myNodeID = nodeid;
            return true;
        }
        if( edge->getID()==myEdge.getEdgeID() &&
            edge->positionLiesWithin(myEdge.getPosition())) {
            cout << "Only the edge lies within the node!!!" << endl;
            cout << "  Querverkehrsstörung-ID: " << myID << endl;
            return true;
        }
        if( edge->getID()==myDisturbance.getEdgeID() &&
            edge->positionLiesWithin(myDisturbance.getPosition())) {
            cout << "Only the disturbing edge lies within the node!!!" << endl;
            cout << "  Querverkehrsstörung-ID: " << myID << endl;
            return true;
        }
    }
    return false;
}*/



bool
NIVissimDisturbance::dictionary(int id,
                                const std::string &name,
                                const NIVissimExtendedEdgePoint &edge,
                                const NIVissimExtendedEdgePoint &by,
                                double timegap, double waygap, double vmax)
{
    int nid = id;
    if(id<0) {
        nid = myRunningID++;
    }
    bool added = false;
    while(true) {
        NIVissimDisturbance *o =
            new NIVissimDisturbance(nid, name, edge, by,
                timegap, waygap, vmax);
        if(!dictionary(nid, o)) {
            delete o;
            nid = myRunningID++;
        } else {
            return true;
        }
    }
}


bool
NIVissimDisturbance::dictionary(int id, NIVissimDisturbance *o)
{
    DictType::iterator i=myDict.find(id);
    if(i==myDict.end()) {
        myDict[id] = o;
        return true;
    }
    return false;
}


NIVissimDisturbance *
NIVissimDisturbance::dictionary(int id)
{
    DictType::iterator i=myDict.find(id);
    if(i==myDict.end()) {
        return 0;
    }
    return (*i).second;
}

/*
void
NIVissimDisturbance::buildNodeClusters()
{
    for(DictType::iterator i=myDict.begin(); i!=myDict.end(); i++) {
        NIVissimDisturbance *e = (*i).second;
        if(!e->clustered()) {
            IntVector disturbances = NIVissimDisturbance::getWithin(*(e->myBoundery));
            IntVector connections = NIVissimConnection::getWithin(*(e->myBoundery));
            int id = NIVissimNodeCluster::dictionary(-1, -1, connections,
                disturbances);
        }
    }
}
*/


/*
IntVector
NIVissimDisturbance::tryAssignToNode(int nodeid,
        const NIVissimNodeParticipatingEdgeVector &edges)
{
    IntVector assigned;
    for(DictType::iterator i=myDict.begin(); i!=myDict.end(); i++) {
        if((*i).second->tryAssignToNodeSingle(nodeid, edges)) {
            assigned.push_back((*i).second->myID);
        }
    }
    return assigned;
}


IntVector
NIVissimDisturbance::getDisturbatorsForEdge(int edgeid)
{
    IntVector ret;
    for(DictType::iterator i=myDict.begin(); i!=myDict.end(); i++) {
        if((*i).second->myEdge.getEdgeID() == edgeid) {
            ret.push_back((*i).second->myID);
        }
    }
    return ret;
}

IntVector
NIVissimDisturbance::getDisturbtionsForEdge(int edgeid)
{
    IntVector ret;
    for(DictType::iterator i=myDict.begin(); i!=myDict.end(); i++) {
        if((*i).second->myDisturbance.getEdgeID() == edgeid) {
            ret.push_back((*i).second->myID);
        }
    }
    return ret;
}
*/

IntVector
NIVissimDisturbance::getWithin(const AbstractPoly &poly)
{
    IntVector ret;
    for(DictType::iterator i=myDict.begin(); i!=myDict.end(); i++) {
        if((*i).second->crosses(poly)) {
            ret.push_back((*i).second->myID);
        }
    }
    return ret;
}


void
NIVissimDisturbance::computeBounding()
{
    assert(myBoundery==0);
    Boundery *bound = new Boundery();
    bound->add(myEdge.getGeomPosition());
    bound->add(myDisturbance.getGeomPosition());
    myBoundery = bound;
    assert(myBoundery!=0&&myBoundery->xmax()>=myBoundery->xmin());
}



bool
NIVissimDisturbance::addToNode(NBNode *node)
{
    myNode = 0;
    NIVissimConnection *pc =
        NIVissimConnection::dictionary(myEdge.getEdgeID());
    NIVissimConnection *bc =
        NIVissimConnection::dictionary(myDisturbance.getEdgeID());
    if(pc==0 && bc==0) {
        // This has not been tested completely, yet
        // Both competing abstract edges are normal edges
        // We have to find a crossing point, build a node here,
        //  split both edges and add the connections
        NIVissimEdge *e1 = NIVissimEdge::dictionary(
            myEdge.getEdgeID());
        NIVissimEdge *e2 = NIVissimEdge::dictionary(
            myDisturbance.getEdgeID());
        Position2D pos = e1->crossesEdgeAtPoint(e2);
        string id1 =
            toString<int>(e1->getID()) + string("x") + toString<int>(e2->getID());
        string id2 =
            toString<int>(e2->getID()) + string("x") + toString<int>(e1->getID());
        NBNode *node1 = NBNodeCont::retrieve(id1);
        NBNode *node2 = NBNodeCont::retrieve(id2);
        NBNode *node = 0;
        assert(node1==0||node2==0);
        if(node1==0&&node2==0) {
			return false;
/*            node = new NBNode(id1, pos.x(), pos.y(), "priority");
            if(!NBNodeCont::insert(node)) {
                cout << "nope, NIVissimDisturbance" << endl;
                throw 1;
            }*/
        } else {
            node = node1==0 ? node2 : node1;
        }
            NBEdgeCont::splitAt(
                NBEdgeCont::retrievePossiblySplitted(
                    toString<int>(e1->getID()), myEdge.getPosition()),
                    node);
            NBEdgeCont::splitAt(
                NBEdgeCont::retrievePossiblySplitted(
                    toString<int>(e2->getID()), myDisturbance.getPosition()),
                    node);
			// !!! in some cases, one of the edges is not being build because it's too short
			// !!! what to do in these cases?
			NBEdge *mayDriveFrom = NBEdgeCont::retrieve(
				toString<int>(e1->getID()) + string("[0]"));
			NBEdge *mayDriveTo = NBEdgeCont::retrieve(
				toString<int>(e1->getID()) + string("[1]"));
			NBEdge *mustStopFrom = NBEdgeCont::retrieve(
				toString<int>(e2->getID()) + string("[0]"));
			NBEdge *mustStopTo = NBEdgeCont::retrieve(
				toString<int>(e2->getID()) + string("[1]"));
			if(mayDriveFrom!=0&&mayDriveTo!=0&&mustStopFrom!=0&&mustStopTo!=0) {
	            node->addSortedLinkFoes(
					NBConnection(mayDriveFrom, mayDriveTo),
					NBConnection(mayDriveFrom, mayDriveTo));
			} else {
				return false;
				// !!! warning
			}
//        }
    } else if(pc!=0 && bc==0) {
        // This has not been tested completely, yet
//        cout << "Warning!!!" << endl;
//        cout << " Unverified usage of edges as prohibitors." << endl;
        // The prohibited abstract edge is a connection, the other
        //  is not;
        // We have to split the other one and add the prohibition
        //  description
        NBEdge *e = NBEdgeCont::retrievePossiblySplitted(
            toString<int>(myDisturbance.getEdgeID()), myDisturbance.getPosition());
        if(e->getFromNode()==e->getToNode()) {
            // What to do with dummy edges?
        } else {
            string nid1 = e->getID() + "[0]";
            string nid2 = e->getID() + "[1]";
            if(NBEdgeCont::splitAt(e, node)) {
                node->addSortedLinkFoes(
                        NBConnection(
                            NBEdgeCont::retrieve(nid1),
                            NBEdgeCont::retrieve(nid2)
                        ),
                        getConnection(node, myEdge.getEdgeID())
                    );
            }
        }
    } else if(bc!=0 && pc==0) {
        // This has not been tested completely, yet
//        cout << "Warning!!!" << endl;
//        cout << " Unverified usage of edges as prohibited." << endl;
        // The prohibiteing abstract edge is a connection, the other
        //  is not;
        // We have to split the other one and add the prohibition
        //  description
        NBEdge *e = NBEdgeCont::retrievePossiblySplitted(
            toString<int>(myEdge.getEdgeID()), myEdge.getPosition());
        string nid1 = e->getID() + "[0]";
        string nid2 = e->getID() + "[1]";
        if( e->getFromNode()==e->getToNode()) {
            // What to do with dummy edges?
            return false;
        } else {
            if(NBEdgeCont::splitAt(e, node)) {
                node->addSortedLinkFoes(
                        getConnection(node, myDisturbance.getEdgeID()),
                        NBConnection(
                            NBEdgeCont::retrieve(nid1),
                            NBEdgeCont::retrieve(nid2)
                        )
                    );
            }
        }
    } else {
        // both the prohibiting and the prohibited abstract edges
        //  are connections
        // We can retrieve the conected edges and add the desription
        NBConnection conn1 = getConnection(node, myDisturbance.getEdgeID());
        NBConnection conn2 = getConnection(node, myEdge.getEdgeID());
        if(!conn1.check()||!conn2.check()) {
            return false;
        }
        node->addSortedLinkFoes(conn1, conn2);
    }
    return true;
}


NBConnection
NIVissimDisturbance::getConnection(NBNode *node, int aedgeid)
{
    if(NIVissimEdge::dictionary(myEdge.getEdgeID())==0) {
        NIVissimConnection *c = NIVissimConnection::dictionary(aedgeid);
        NBEdge *from =
            node->getPossiblySplittedIncoming(toString<int>(c->getFromEdgeID()));
        NBEdge *to =
            node->getPossiblySplittedOutgoing(toString<int>(c->getToEdgeID()));

        // source is a connection
        return NBConnection(toString<int>(c->getFromEdgeID()), from,
            toString<int>(c->getToEdgeID()), to);
    } else {
        cout << "NIVissimDisturbance: no connection" << endl;
        return NBConnection(0, 0);
//        throw 1; // !!! what to do?
    }

}

void
NIVissimDisturbance::clearDict()
{
    for(DictType::iterator i=myDict.begin(); i!=myDict.end(); i++) {
        delete (*i).second;
    }
    myDict.clear();
}


void
NIVissimDisturbance::dict_SetDisturbances()
{
    for(DictType::iterator i=myDict.begin(); i!=myDict.end(); i++) {
        delete (*i).second;
    }
}





