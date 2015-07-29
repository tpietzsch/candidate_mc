/**
 * Reads a treemc project file containing features and a ground-truth labelling 
 * and trains node and edge feature weights.
 */

#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>

#include <util/Logger.h>
#include <util/ProgramOptions.h>
#include <util/exceptions.h>
#include <io/Hdf5CragStore.h>
#include <io/Hdf5VolumeStore.h>
#include <io/vectors.h>
#include <learning/BundleOptimizer.h>
#include <learning/Oracle.h>
#include <learning/OverlapLoss.h>
#include <learning/HammingLoss.h>
#include <learning/BestEffort.h>

util::ProgramOption optionProjectFile(
		util::_long_name        = "projectFile",
		util::_short_name       = "p",
		util::_description_text = "The treemc project file.",
		util::_default_value    = "project.hdf");

util::ProgramOption optionFeatureWeights(
		util::_long_name        = "featureWeights",
		util::_short_name       = "w",
		util::_description_text = "A file to store the learnt feature weights.",
		util::_default_value    = "feature_weights.txt");

util::ProgramOption optionBestEffortFromProjectFile(
		util::_long_name        = "bestEffortFromProjectFile",
		util::_description_text = "Read the best effort solution from the project file.");

util::ProgramOption optionLoss(
		util::_long_name        = "loss",
		util::_description_text = "The loss to use for training: hamming (Hamming distance "
		                          "to best effort, default) or overlap (RAND index approximation "
		                          "to ground-truth).",
		util::_default_value    = "hamming");

util::ProgramOption optionNormalizeLoss(
		util::_long_name        = "normalizeLoss",
		util::_description_text = "Normalize the loss, such that values on valid solutions are in [0,1].");

util::ProgramOption optionRegularizerWeight(
		util::_long_name        = "regularizerWeight",
		util::_description_text = "The factor of the quadratic regularizer on w.",
		util::_default_value    = 1.0);

util::ProgramOption optionInitialWeightValues(
		util::_long_name        = "initialWeightValues",
		util::_description_text = "Uniform values of the weight vectors to start learning with.",
		util::_default_value    = 0);

util::ProgramOption optionInitialWeights(
		util::_long_name        = "initialWeights",
		util::_description_text = "A file containing an initial set of weights.");

util::ProgramOption optionPretrain(
		util::_long_name        = "pretrain",
		util::_description_text = "Train on a much simpler version of the original problem to get an "
		                          "SVM-like training of the feature weights.");

int main(int argc, char** argv) {

	try {

		util::ProgramOptions::init(argc, argv);
		logger::LogManager::init();

		Hdf5CragStore   cragStore(optionProjectFile.as<std::string>());
		Hdf5VolumeStore volumeStore(optionProjectFile.as<std::string>());

		Crag crag;
		CragVolumes volumes(crag);
		cragStore.retrieveCrag(crag);
		cragStore.retrieveVolumes(volumes);

		NodeFeatures nodeFeatures(crag);
		EdgeFeatures edgeFeatures(crag);

		LOG_USER(logger::out) << "reading features" << std::endl;
		cragStore.retrieveNodeFeatures(crag, nodeFeatures);
		cragStore.retrieveEdgeFeatures(crag, edgeFeatures);

		BundleOptimizer::Parameters parameters;
		parameters.lambda      = optionRegularizerWeight;
		parameters.epsStrategy = BundleOptimizer::EpsFromGap;
		BundleOptimizer optimizer(parameters);

		BestEffort*  bestEffort  = 0;
		OverlapLoss* overlapLoss = 0;

		if (optionBestEffortFromProjectFile) {

			LOG_USER(logger::out) << "reading best-effort" << std::endl;

			bestEffort = new BestEffort(crag);

			vigra::HDF5File project(
					optionProjectFile.as<std::string>(),
					vigra::HDF5File::OpenMode::ReadWrite);
			project.cd("best_effort");
			vigra::ArrayVector<int> beNodes;
			vigra::MultiArray<2, int> beEdges;
			project.readAndResize("nodes", beNodes);
			project.readAndResize("edges", beEdges);

			std::set<int> nodes;
			for (int n : beNodes)
				nodes.insert(n);

			std::set<std::pair<int, int>> edges;
			for (int i = 0; i < beEdges.shape(1); i++)
				edges.insert(
						std::make_pair(
							std::min(beEdges(i, 0), beEdges(i, 1)),
							std::max(beEdges(i, 0), beEdges(i, 1))));

			for (Crag::NodeIt n(crag); n != lemon::INVALID; ++n)
				bestEffort->node[n] = nodes.count(crag.id(n));

			for (Crag::EdgeIt e(crag); e != lemon::INVALID; ++e)
				bestEffort->edge[e] = edges.count(
						std::make_pair(
								std::min(crag.id(crag.u(e)), crag.id(crag.v(e))),
								std::max(crag.id(crag.u(e)), crag.id(crag.v(e)))));

		} else {

			LOG_USER(logger::out) << "reading ground-truth" << std::endl;
			ExplicitVolume<int> groundTruth;
			volumeStore.retrieveGroundTruth(groundTruth);

			LOG_USER(logger::out) << "finding best-effort solution" << std::endl;
			overlapLoss = new OverlapLoss(crag, volumes, groundTruth);
			bestEffort = new BestEffort(crag, volumes, *overlapLoss);
		}

		Loss* loss = 0;
		bool  destructLoss = false;

		if (optionLoss.as<std::string>() == "hamming") {

			LOG_USER(logger::out) << "using Hamming loss" << std::endl;

			loss = new HammingLoss(crag, *bestEffort);
			destructLoss = true;

		} else if (optionLoss.as<std::string>() == "overlap") {

			LOG_USER(logger::out) << "using overlap loss" << std::endl;

			if (!overlapLoss) {

				LOG_USER(logger::out) << "reading ground-truth" << std::endl;
				ExplicitVolume<int> groundTruth;
				volumeStore.retrieveGroundTruth(groundTruth);

				LOG_USER(logger::out) << "finding best-effort solution" << std::endl;
				overlapLoss = new OverlapLoss(crag, volumes, groundTruth);
			}

			loss = overlapLoss;

		} else {

			LOG_ERROR(logger::out)
					<< "unknown loss: "
					<< optionLoss.as<std::string>()
					<< std::endl;
			return 1;
		}

		MultiCut::Parameters multiCutParameters;
		if (optionPretrain)
			multiCutParameters.noConstraints = true;

		if (optionNormalizeLoss) {

			LOG_USER(logger::out) << "normalizing loss..." << std::endl;
			loss->normalize(crag, multiCutParameters);
		}

		Oracle oracle(
				crag,
				volumes,
				nodeFeatures,
				edgeFeatures,
				*loss,
				*bestEffort,
				multiCutParameters);

		std::vector<double> weights;

		if (!optionInitialWeights) {

			weights.resize(nodeFeatures.dims() + edgeFeatures.dims(), optionInitialWeightValues.as<double>());

		} else {

			weights = retrieveVector<double>(optionInitialWeights.as<std::string>());

			if (weights.size() != nodeFeatures.dims() + edgeFeatures.dims())
				UTIL_THROW_EXCEPTION(
						UsageError,
						"provided feature weights file has wrong number of entries " <<
						"(" << weights.size() << ", should be " << nodeFeatures.dims() + edgeFeatures.dims());
		}

		optimizer.optimize(oracle, weights);

		storeVector(weights, optionFeatureWeights);

		if (destructLoss && loss != 0)
			delete loss;

		if (overlapLoss)
			delete overlapLoss;

		if (bestEffort)
			delete bestEffort;

	} catch (boost::exception& e) {

		handleException(e, std::cerr);
	}
}


