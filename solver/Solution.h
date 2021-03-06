#ifndef INFERENCE_SOLUTION_H__
#define INFERENCE_SOLUTION_H__

#include <vector>

class Solution {

public:

	Solution(unsigned int size = 0);

	void resize(unsigned int size);

	unsigned int size() const { return _solution.size(); }

	const double& operator[](unsigned int i) const { return _solution[i]; }

	double& operator[](unsigned int i) { return _solution[i]; }

	std::vector<double>& getVector() { return _solution; }

	void setValue(double value) { _value = value; }

	double getValue() const { return _value; }

private:

	std::vector<double> _solution;

	double _value;
};

#endif // INFERENCE_SOLUTION_H__

