#ifndef YAMLSERVICES_H 
#define YAMLSERVICES_H




#include <string>
#include <vector>

class YamlData
{
	public:
	int rows;
	int cols;
	int **p;
	std::vector<std::vector<unsigned> > yamlData;
	public:
	YamlData();
	bool parseYamlData(std::string path);
	void printYamlData();
	int data(int i ,int j);
};


#endif
