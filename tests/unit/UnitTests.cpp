#include <iostream>

bool UnitTest_Dummy(int argc, char** argv) {
	try
	{
		// do unit test
		return true;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Exception in UnitTest_Dummy" << std::endl;
		std::cerr << e.what() << std::endl;
		return false;
	}
}

// Handy macro from: s2client-api/tests/all_tests.cc
#define TEST(X)                                                     \
    std::cout << "Running unit test: " << #X << std::endl;          \
    if (X(argc, argv)) {                                            \
        std::cout << "Test: " << #X << " succeeded." << std::endl;  \
    }                                                               \
    else {                                                          \
        success = false;                                            \
        std::cerr << "Test: " << #X << " failed!" << std::endl;     \
    }

int main(int argc, char** argv) {
	bool success = true;

	TEST(UnitTest_Dummy);
	// Add more tests here...

	if (success)
		std::cout << "All unit tests succeeded!" << std::endl;
	else
		std::cerr << "Some unit tests failed!" << std::endl;

	return success ? 0 : -1;
}