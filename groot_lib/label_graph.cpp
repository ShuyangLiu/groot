#include <boost/algorithm/string.hpp>
#include "graph.h"
#include "interpreter.h"
#include "properties.h"

std::map<VertexDescriptor, LabelMap> gDomainChildLabelMap;


std::size_t hash_value(Label const& l)
{
	boost::hash<boost::flyweight<std::string, boost::flyweights::no_locking, boost::flyweights::no_tracking>> hasher;
	return hasher(l.n);
}

LabelMap ConstructLabelMap(const LabelGraph& g, VertexDescriptor node) {
	LabelMap m;
	for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(node, g))) {
		if (g[edge].type == normal) {
			m[g[edge.m_target].name] = edge.m_target;
		}
	}
	return m;
}


VertexDescriptor GetClosestEncloser(const LabelGraph& g, VertexDescriptor root, vector<Label> labels, int& index) {
	VertexDescriptor closestEncloser = root;
	if (labels.size() == index) {
		return closestEncloser;
	}
	if (out_degree(closestEncloser, g) > kHashMapThreshold) {
		if (gDomainChildLabelMap.find(closestEncloser) == gDomainChildLabelMap.end()) {
			gDomainChildLabelMap[closestEncloser] = ConstructLabelMap(g, closestEncloser);
		}
		LabelMap& m = gDomainChildLabelMap.find(closestEncloser)->second;
		auto it = m.find(labels[index]);
		if (it != m.end()) {
			closestEncloser = it->second;
			index++;
			return GetClosestEncloser(g, closestEncloser, labels, index);
		}
	}
	else {
		for (EdgeDescriptor edge : boost::make_iterator_range(out_edges(closestEncloser, g))) {
			if (g[edge].type == normal) {
				if (g[edge.m_target].name == labels[index]) {
					closestEncloser = edge.m_target;
					index++;
					return GetClosestEncloser(g, closestEncloser, labels, index);
				}
			}
		}
	}
	return closestEncloser;
}


VertexDescriptor AddNodes(LabelGraph& g, VertexDescriptor closetEncloser, vector<Label> labels, int& index) {
	
	for (int i = index; i < labels.size(); i++) {
		VertexDescriptor u = boost::add_vertex(g);
		g[u].name = labels[i];
		EdgeDescriptor e; bool b;
		boost::tie(e, b) = boost::add_edge(closetEncloser, u, g);
		if (!b) {
			cout << "Unable to add edge" << endl;
			exit(EXIT_FAILURE);
		}
		g[e].type = normal;
		//Only the first closestEncloser might have a map. For other cases closestEncloser will take care.
		if (gDomainChildLabelMap.find(closetEncloser) != gDomainChildLabelMap.end()) {
			LabelMap& m = gDomainChildLabelMap.find(closetEncloser)->second;
			m[labels[i]] = u;
		}
		closetEncloser = u;
	}
	return closetEncloser;
}

void LabelGraphBuilder(ResourceRecord& record, LabelGraph& g, const VertexDescriptor root) {
	
	if (record.get_type() != RRType::N) {
		int index = 0;
		VertexDescriptor closetEncloser = GetClosestEncloser(g, root, record.get_name(), index);
		VertexDescriptor mainNode = AddNodes(g, closetEncloser, record.get_name(), index);
		if (record.get_type() == RRType::DNAME) {
			vector<Label> labels = GetLabels(record.get_rdata());
			index = 0;
			closetEncloser = GetClosestEncloser(g, root, labels, index);
			VertexDescriptor secondNode = AddNodes(g, closetEncloser, labels, index);
			EdgeDescriptor e; bool b;
			boost::tie(e, b) = boost::add_edge(mainNode, secondNode, g);
			if (!b) {
				cout << "Unable to add edge" << endl;
				exit(EXIT_FAILURE);
			}
			g[e].type = dname;
		}
	}
}

void LabelGraphBuilder(vector<ResourceRecord>& rrs, LabelGraph& g, const VertexDescriptor root) {

	//Assumption : All RR's have the owner names, CNAMEs and DNAMEs as Fully Quantified Domain Name.
	for (auto& record : rrs)
	{
		LabelGraphBuilder(record, g, root);
	}
}