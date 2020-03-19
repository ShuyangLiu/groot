#pragma once

#include <iostream>
#include <string>
#include <unordered_map> 
#include <vector>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/serialization/bitset.hpp>
#include <boost/serialization/optional.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

#include "resource_record.h"
#include "graph.h"

using namespace std;

enum class ReturnTag
{
	ANS,
	REWRITE,
	REF,
	NX,
	REFUSED,
	NSNOTFOUND
};

typedef tuple<ReturnTag, std::bitset<RRType::N>, vector<ResourceRecord>> ZoneLookUpAnswer;

//Zone Graph
struct ZoneVertex {
	Label name;
	vector<ResourceRecord> rrs;
private:
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version) {
		ar& name;
		ar& rrs;
	}
};

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, ZoneVertex> ZoneGraph;
typedef boost::graph_traits<ZoneGraph>::vertex_descriptor ZoneVertexDescriptor;
typedef boost::graph_traits<ZoneGraph>::edge_descriptor ZoneEdgeDescriptor;


struct Zone {
	ZoneGraph g;
	int zoneId;
	ZoneVertexDescriptor startVertex =0;
	vector<Label> origin;
	std::unordered_map<VertexDescriptor, LabelMap> domainChildLabelMap;
private:
	friend class boost::serialization::access;
	template <typename Archive>
	void serialize(Archive& ar, const unsigned int version) {
		ar& g;
		ar& startVertex;
		ar& origin;
	}
};

extern std::unordered_map<string, std::vector<int>> gNameServerZoneMap;
extern std::vector<string> gTopNameServers;
extern std::unordered_map<int, Zone> gZoneIdToZoneMap;
extern int zoneId;

string SearchForNameServer(int zoneId);
bool RequireGlueRecords(Zone z, vector<ResourceRecord>& NSRecords);

//Zone parser
void ParseZoneFile(string& file, LabelGraph& g, const VertexDescriptor& root, Zone& z);

//ZoneLookUp
vector<ResourceRecord> GlueRecordsLookUp(ZoneGraph& g, ZoneVertexDescriptor root, vector<ResourceRecord>& NSRecords, std::unordered_map<VertexDescriptor, LabelMap>& domainChildLabelMap);
boost::optional<ZoneVertexDescriptor> ZoneGraphBuilder(ResourceRecord& record, Zone& z);
void ZoneGraphBuilder(vector<ResourceRecord>& rrs, Zone& z);
string ReturnTagToString(ReturnTag& r);
void BuildZoneLabelGraphs(string filePath, string nameServer, LabelGraph& g, const VertexDescriptor& root);
boost::optional<vector<ZoneLookUpAnswer>> QueryLookUpAtZone(Zone& z, EC& query, bool& completeMatch);
