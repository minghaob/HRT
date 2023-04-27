#include "common.h"

namespace util
{

uint32_t GetStringEditDistance(const std::string& first, const std::string& second, uint32_t max_allowed_edits)
{
	uint32_t m = uint32_t(first.length());
	uint32_t n = uint32_t(second.length());
	uint32_t s = max_allowed_edits;

	std::vector<uint32_t> T((m + 1) * (n + 1));		// TODO: make this thread_local to avoid multiple memory allocations, or just use alloca()
	T[0] = 0;
	for (uint32_t i = 1; i <= std::min(s, n); i++) {
		T[i * (n + 1) + 0] = i;
	}

	for (uint32_t j = 1; j <= std::min(s, m); j++) {
		T[0 * (n + 1) + j] = j;
	}

	for (uint32_t i = 1; i <= m; i++) {
		uint32_t startColumn = std::max(i, s) - s + 1;
		if (startColumn >= (n + 1))
			return max_allowed_edits + 1;
		uint32_t endColumn = std::min(i + s, n);
		uint32_t minInRow = T[i * (n + 1)];
		if (i > s)
			T[i * (n + 1) + startColumn - 1] = max_allowed_edits;
		if (i + s <= n)
			T[(i - 1) * (n + 1) + endColumn] = max_allowed_edits;
		for (uint32_t j = startColumn; j <= endColumn; j++) {
			uint32_t weight = first[i - 1] == second[j - 1] ? 0 : 1;
			T[i * (n + 1) + j] = std::min(std::min(T[(i - 1) * (n + 1) + j] + 1, T[i * (n + 1) + j - 1] + 1), T[(i - 1) * (n + 1) + j - 1] + weight);
			minInRow = std::min(minInRow, T[i * (n + 1) + j]);
		}
		if (minInRow > max_allowed_edits)
			return max_allowed_edits + 1;
	}

	return T[m * (n + 1) + n];
}

void OpenCvMatBGRAToLeptonicaRGBAInplace(cv::Mat& frame)
{
	//                 byte[0] byte[1] byte[2] byte[3]
	// opencv BGRA        B       G       R       A		(little-endian)
	// leptonica RGBA     A       B       G       R     (big-endian)
	for (int row = 0; row < frame.rows; row++)
	{
		uint32_t* data = (uint32_t*)frame.row(row).data;
		for (int col = 0; col < frame.cols; col++)
			data[col] = (data[col] << 8) | (data[col] >> 24);
	}
}

}
