#include <iostream>
#include <tests.h>
#include <util/ProgramOptions.h>
#include <util/Logger.h>
#include <util/exceptions.h>

BEGIN_TEST_MODULE(candidate_mc)

	ADD_TEST_SUITE(crag);
	ADD_TEST_SUITE(features);
	ADD_TEST_SUITE(solver);
	ADD_TEST_SUITE(learning);
	ADD_TEST_SUITE(io);
	ADD_TEST_SUITE(third_party);

END_TEST_MODULE()

void exceptionTranslator(const Exception& error) {

	handleException(error, std::cout);
	throw error;
}

int main(int argc, char** argv) {

	util::ProgramOptions::init(argc, argv, true);
	logger::LogManager::init();

	::boost::unit_test::unit_test_monitor.register_exception_translator<Exception>(&exceptionTranslator);

	return ::boost::unit_test::unit_test_main(&candidate_mc, argc, argv);
}
