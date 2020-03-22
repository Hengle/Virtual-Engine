#include "IEnumerator.h"
bool IEnumerator::ExecuteOne()
{
	if (startIndex >= executors.size()) return false;
	if (executors[startIndex]())
	{
		startIndex++;
	}
	return true;
}