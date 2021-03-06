#ifndef CANDIDATE_MC_CRAG_CRAG_STACK_COMBINER_H__
#define CANDIDATE_MC_CRAG_CRAG_STACK_COMBINER_H__

#include <memory>
#include <vector>
#include "Crag.h"
#include "CragVolumes.h"

/**
 * Combines a stack of CRAGs(coming from a stack of images) into a single crag.
 */
class CragStackCombiner {

public:

	CragStackCombiner();

	void combine(
			const std::vector<std::unique_ptr<Crag>>&        sourcesCrags,
			const std::vector<std::unique_ptr<CragVolumes>>& sourcesVolumes,
			Crag&                                            targetCrag,
			CragVolumes&                                     targetVolumes);

private:

	std::map<Crag::CragNode, Crag::CragNode> copyNodes(
			unsigned int       z,
			const Crag&        source,
			const CragVolumes& sourceVolumes,
			Crag&              target,
			CragVolumes&       targetVolumes);

	std::vector<std::pair<Crag::CragNode, Crag::CragNode>> findLinks(const Crag& a, const Crag& b);

	std::vector<std::pair<Crag::CragNode, Crag::CragNode>> findLinks(
			const Crag&        cragA,
			const CragVolumes& volsA,
			const Crag&        cragB,
			const CragVolumes& volsB);

	double _maxDistance;

	bool _requireBbOverlap;

	std::map<Crag::CragNode, Crag::CragNode> _prevNodeMap;
	std::map<Crag::CragNode, Crag::CragNode> _nextNodeMap;

	std::vector<Crag::CragNode> _noAssignmentNodes;
};

#endif // CANDIDATE_MC_CRAG_CRAG_STACK_COMBINER_H__

