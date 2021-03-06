/*
 *  ofxArtNet.cpp
 *  artnetExample
 *
 *  Created by Tobias Ebsen on 9/25/12.
 *  Copyright 2012 Tobias Ebsen. All rights reserved.
 *
 */

#include "ofxArtNet.h"
#include "artnet.h"

string ofxArtNet::getIP(){
    auto list =  LocalAddressGrabber :: availableList();
    for(auto iface : list){
        if(ofStringTimesInString(iface, "en") != 0){
            return LocalAddressGrabber :: getIpAddress(iface);
        }
    }
}

vector<pair<string, string>> ofxArtNet::getIfacesIps(){
    vector<pair<string , string>> ifacesPair;
    vector<string> ifaces = LocalAddressGrabber :: availableList();
    for (auto iface : ifaces)
        ifacesPair.push_back(pair<string, string>(iface, LocalAddressGrabber :: getIpAddress(iface)));
    
    return ifacesPair;
}

////////////////////////////////////////////////////////////
void ofxArtNet::init(string ip, bool verbose) {
    if(ip == "") ip = getIP();
	node = artnet_new((ip.empty() ? NULL : ip.c_str()), verbose);
	if (node == NULL) {
		ofLog(OF_LOG_ERROR, artnet_strerror());
		return;
	}
	artnet_set_handler(node, ARTNET_REPLY_HANDLER, ofxArtNet::reply_handler, this);
	artnet_set_dmx_handler(node, ofxArtNet::dmx_handler, this);
}
////////////////////////////////////////////////////////////
void ofxArtNet::setOEM(int high, int low) {
	artnet_setoem(node, high, low);
}
////////////////////////////////////////////////////////////
void ofxArtNet::setNodeType(artnetNodeType nodeType) {
	artnet_set_node_type(node, (artnet_node_type)nodeType);
}
////////////////////////////////////////////////////////////
void ofxArtNet::setShortName(string shortname) {
	artnet_set_short_name(node, shortname.c_str());
}
////////////////////////////////////////////////////////////
void ofxArtNet::setLongName(string longname) {
	artnet_set_long_name(node, longname.c_str());
}
////////////////////////////////////////////////////////////
void ofxArtNet::setSubNet(int subnet) {
	artnet_set_subnet_addr(node, subnet);
}
////////////////////////////////////////////////////////////
void ofxArtNet::setBroadcastLimit(int bcastlimit) {
	artnet_set_bcast_limit(node, bcastlimit);
}
////////////////////////////////////////////////////////////
void ofxArtNet::setPortType(int port, artnetPortIO io, artnetPortData data) {
	artnet_set_port_type(node, port, (artnet_port_settings_t)io, (artnet_port_data_code)data);
}
////////////////////////////////////////////////////////////
void ofxArtNet::setPortAddress(int port, artnetPortType type, unsigned char address) {
	artnet_set_port_addr(node, port, (artnet_port_dir_t)type, address);
}
////////////////////////////////////////////////////////////
void ofxArtNet::start() {
	if (artnet_start(node) != ARTNET_EOK) {
		ofLog(OF_LOG_ERROR, artnet_strerror());
		return;
	}
	startThread();
}
////////////////////////////////////////////////////////////
void ofxArtNet::stop() {
	stopThread();
	artnet_stop(node);
}
////////////////////////////////////////////////////////////
void ofxArtNet::close() {
	artnet_destroy(node);
	node = NULL;
}
////////////////////////////////////////////////////////////
void ofxArtNet::sendPoll(string ip) {
	for (int i=0; i<nodes.size(); i++)
		delete nodes[i];
	nodes.clear();
	artnet_send_poll(node, ip.empty() ? NULL : ip.c_str(), ARTNET_TTM_DEFAULT);
}
////////////////////////////////////////////////////////////
void ofxArtNet::sendDmx(int port, const char* targetIp, void* data, int size) {
	artnet_send_dmx(node, port, targetIp, size, (unsigned char*)data);
}

////////////////////////////////////////////////////////////
void ofxArtNet::sendDmx_by_SU(int port,int subnet, int universe, const char* targetIp, void* data, int size) {
    artnet_send_dmx_by_custom_SU(node, port, subnet, universe, targetIp, size, (unsigned char*)data);
    //cout<<universe<<endl;
}

////////////////////////////////////////////////////////////
void ofxArtNet::sendDmx(ofxArtNetDmxData& dmx) {
//	sendDmx(dmx.getPort(), dmx.getIp().c_str() ,dmx.getData().data(), dmx.getLen());
    sendDmx_by_SU(dmx.getPort(), dmx.getSubNet(), dmx.getUniverse(), dmx.getIp().c_str() ,dmx.getData().data(), dmx.getLen());
}
////////////////////////////////////////////////////////////
void ofxArtNet::sendDmxRaw(int universe, void* data, int size) {
	artnet_raw_send_dmx(node, universe, size, (unsigned char*)data);
}
////////////////////////////////////////////////////////////
void ofxArtNet::threadedFunction() {
	
	fd_set rd_fds;
	struct timeval tv;
	int max;
	
	int artnet_sd = artnet_get_sd(node);
	
	while (isThreadRunning()) {

		FD_ZERO(&rd_fds);
		FD_SET(0, &rd_fds);
		FD_SET(artnet_sd, &rd_fds);
		
		max = artnet_sd;

		tv.tv_sec = 1;
		tv.tv_usec = 0;
		
		int n = select(max+1, &rd_fds, NULL, NULL, &tv);
		if (n > 0) {
			if (FD_ISSET(artnet_sd , &rd_fds))
	    		artnet_read(node, 0);
		}
	}
}
////////////////////////////////////////////////////////////
int ofxArtNet::reply_handler(artnet_node node, void *pp, void *d) {

	ofxArtNet* t = (ofxArtNet*)d;
	artnet_node_list nl = artnet_get_nl(node);
	int nodes_found = t->nodes.size();

	int n = artnet_nl_get_length(nl);
	if(n == nodes_found)
		return 0;
	
	artnet_node_entry ne = NULL;
	if(nodes_found == 0)
		ne = artnet_nl_first(nl);
	else
		ne = artnet_nl_next(nl);

	ofxArtNetNodeEntry* entry = new ofxArtNetNodeEntry(ne);
	t->nodes.push_back(entry);
	ofNotifyEvent(t->pollReply, *entry, t);
	
	return 0;
}
////////////////////////////////////////////////////////////
int ofxArtNet::dmx_handler(artnet_node n, int port, void *d) {
	
	ofxArtNet* t = (ofxArtNet*)d;
	
	int len;
	unsigned char* data = artnet_read_dmx(n, port, &len);
    vector<unsigned char> dataVec(&data[0], &data[len]);
//    dataVec.assign(len, sizeof(unsigned char));
//    dataVec = *data;
	ofxArtNetDmxData dmx(dataVec, len);
	dmx.setPort(port);
	ofNotifyEvent(t->dmxData, dmx, t);
}
