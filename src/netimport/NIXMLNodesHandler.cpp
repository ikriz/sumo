/****************************************************************************/
/// @file    NIXMLNodesHandler.cpp
/// @author  Daniel Krajzewicz
/// @date    Tue, 20 Nov 2001
/// @version $Id$
///
// Importer for network nodes stored in XML
/****************************************************************************/
// SUMO, Simulation of Urban MObility; see http://sumo.sourceforge.net/
// Copyright (C) 2001-2011 DLR (http://www.dlr.de/) and contributors
/****************************************************************************/
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//
/****************************************************************************/


// ===========================================================================
// included modules
// ===========================================================================
#ifdef _MSC_VER
#include <windows_config.h>
#else
#include <config.h>
#endif

#include <string>
#include <iostream>
#include <xercesc/sax/HandlerBase.hpp>
#include <xercesc/sax/AttributeList.hpp>
#include <xercesc/sax/SAXParseException.hpp>
#include <xercesc/sax/SAXException.hpp>
#include <utils/xml/SUMOSAXHandler.h>
#include <utils/xml/SUMOXMLDefinitions.h>
#include <utils/common/MsgHandler.h>
#include <utils/common/TplConvert.h>
#include <utils/common/ToString.h>
#include <utils/common/StringTokenizer.h>
#include <utils/options/OptionsCont.h>
#include <utils/geom/GeoConvHelper.h>
#include <netbuild/NBNodeCont.h>
#include <netbuild/NBTrafficLightLogicCont.h>
#include <netbuild/NBOwnTLDef.h>
#include "NIXMLNodesHandler.h"

#ifdef CHECK_MEMORY_LEAKS
#include <foreign/nvwa/debug_new.h>
#endif // CHECK_MEMORY_LEAKS


// ===========================================================================
// method definitions
// ===========================================================================
NIXMLNodesHandler::NIXMLNodesHandler(NBNodeCont &nc,
                                     NBTrafficLightLogicCont &tlc,
                                     OptionsCont &options)
        : SUMOSAXHandler("xml-nodes - file"),
        myOptions(options),
        myNodeCont(nc), myTLLogicCont(tlc) {}


NIXMLNodesHandler::~NIXMLNodesHandler() throw() {}


void
NIXMLNodesHandler::myStartElement(int element,
                                  const SUMOSAXAttributes &attrs) throw(ProcessError) {
    if (element!=SUMO_TAG_NODE) {
        return;
    }
    bool ok = true;
    // get the id, report a warning if not given or empty...
    myID = attrs.getStringReporting(SUMO_ATTR_ID, 0, ok);
    if (!ok) {
        return;
    }
    NBNode *node = myNodeCont.retrieve(myID);
    // retrieve the position of the node
    bool xOk = false;
    bool yOk = false;
    bool needConversion = true;
    if (node!=0) {
        myPosition = node->getPosition();
        xOk = yOk = true;
        needConversion = false;
    }
    if (attrs.hasAttribute(SUMO_ATTR_X)) {
        myPosition.set(attrs.getSUMORealReporting(SUMO_ATTR_X, myID.c_str(), ok), myPosition.y());
        xOk = true;
    }
    if (attrs.hasAttribute(SUMO_ATTR_Y)) {
        myPosition.set(myPosition.x(), attrs.getSUMORealReporting(SUMO_ATTR_Y, myID.c_str(), ok));
        yOk = true;
    }
    if (xOk&&yOk) {
        if (needConversion&&!GeoConvHelper::x2cartesian(myPosition)) {
            MsgHandler::getErrorInstance()->inform("Unable to project coordinates for node '" + myID + "'.");
        }
    } else {
        MsgHandler::getErrorInstance()->inform("Missing position (at node ID='" + myID + "').");
    }
    // check whether the y-axis shall be flipped
    if (myOptions.getBool("flip-y-axis")) {
        myPosition.mul(1.0, -1.0);
    }
    // get the type
    SumoXMLNodeType type = NODETYPE_UNKNOWN;
    if (node!=0) {
        type = node->getType();
    }
    std::string typeS = attrs.getOptStringReporting(SUMO_ATTR_TYPE, myID.c_str(), ok, "");
    if (SUMOXMLDefinitions::NodeTypes.hasString(typeS)) {
        type = SUMOXMLDefinitions::NodeTypes.get(typeS);
    }

    // check whether a prior node shall be modified
    if (node==0) {
        node = new NBNode(myID, myPosition, type);
        if (!myNodeCont.insert(node)) {
            throw ProcessError("Could not insert node though checked this before (id='" + myID + "').");
        }
    } else {
        // remove previously set tls if this node is not controlled by a tls
        std::set<NBTrafficLightDefinition*> tls = node->getControllingTLS();
        node->removeTrafficLights();
        for (std::set<NBTrafficLightDefinition*>::iterator i=tls.begin(); i!=tls.end(); ++i) {
            if ((*i)->getNodes().size()==0) {
                myTLLogicCont.removeFully((*i)->getID());
            }
        }
        // patch information
        node->reinit(myPosition, type);
    }
    // process traffic light definition
    if (type==NODETYPE_TRAFFIC_LIGHT) {
        processTrafficLightDefinitions(attrs, node);
    }
}


void
NIXMLNodesHandler::processTrafficLightDefinitions(const SUMOSAXAttributes &attrs,
        NBNode *currentNode) {
    // try to get the tl-id
    // if a tl-id is given, we will look whether this tl already exists
    //  if so, we will add the node to it, otherwise allocate a new one with this id
    // if no tl-id exists, we will build a tl with the node's id
    NBTrafficLightDefinition *tlDef = 0;
    bool ok = true;
    std::string tlID = attrs.getOptStringReporting(SUMO_ATTR_TLID, 0, ok, "");
    if (tlID!="") {
        // ok, the traffic light has a name
        const std::map<std::string, NBTrafficLightDefinition*>& programs = myTLLogicCont.getPrograms(tlID);
        if (programs.size() == 0) {
            // this traffic light is visited the first time
            tlDef = new NBOwnTLDef(tlID, currentNode);
            if (!myTLLogicCont.insert(tlDef)) {
                // actually, nothing should fail here
                delete tlDef;
                throw ProcessError("Could not allocate tls '" + tlID + "'.");
            }
        } else {
            std::map<std::string, NBTrafficLightDefinition*>::const_iterator it;
            for (it = programs.begin(); it!= programs.end(); it++) {
                it->second->addNode(currentNode);
            }
        }
    } else {
        // ok, this node is a traffic light node where no other nodes
        //  participate
        tlDef = new NBOwnTLDef(myID, currentNode);
        if (!myTLLogicCont.insert(tlDef)) {
            // actually, nothing should fail here
            delete tlDef;
            throw ProcessError("Could not allocate tls '" + myID + "'.");
        }
    }
    // process inner edges which shall be controlled
    std::vector<std::string> controlledInner;
	if(attrs.hasAttribute(SUMO_ATTR_CONTROLLED_INNER__DEPRECATED)) {
	    SUMOSAXAttributes::parseStringVector(attrs.getStringReporting(SUMO_ATTR_CONTROLLED_INNER__DEPRECATED, 0, ok), controlledInner);
	} else {
	    SUMOSAXAttributes::parseStringVector(attrs.getOptStringReporting(SUMO_ATTR_CONTROLLED_INNER, 0, ok, ""), controlledInner);
	}
    if (controlledInner.size()!=0) {
        tlDef->addControlledInnerEdges(controlledInner);
    }
}



/****************************************************************************/

