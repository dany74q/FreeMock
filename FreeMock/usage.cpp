#include "freemock.h"

using namespace freemock;

void func(int, double) {
	std::printf("original");
}

void mocked(int, double) {
	std::printf("free function mock\n");
}

void withFreeFunction() {
	auto dummyMocked = make_mock(func, mocked);
	func(0, 0.5);
}

void withLambda() {
	auto dummyMocked = make_mock(func, [](int, double) { std::printf("lambda mock\n");});
	func(0, 0.5);
}

void withMutableLambda() {
	auto dummyMocked = make_mock(func, [](int, double) mutable { std::printf("mutable lambda mock\n");});
	func(0, 0.5);
}

int main() {
	// Will print "free function mock"
	withFreeFunction();
	// Will print "lambda mock"
	withLambda();
	// Will print "mutable lambda mock"
	withMutableLambda();
	// Will print "original"
	func(1, 1.0);
}