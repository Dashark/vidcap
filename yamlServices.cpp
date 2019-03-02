#include "yamlServices.h"
#include<yaml-cpp/yaml.h>

YamlData::YamlData()
{
	rows = 0;
	cols = 0;
}
bool YamlData::parseYamlData(std::string fileUrl){
	
	if (fileUrl.empty()){
		return false;
	}
	printf("fileUrl: %s\n", fileUrl.c_str());
	
	YAML::Node yamlfile = YAML::LoadFile(fileUrl);
	if (yamlfile.IsNull()){
		return false;
	}
    
    YAML::Node label = yamlfile["label"];
	
	rows = label["rows"].as<int>();
	cols = label["cols"].as<int>();
	printf("rows: %d, cols: %d\n", rows, cols);
	std::vector<unsigned> data = label["data"].as<std::vector<unsigned>>();
	
	if (data.size() != rows * cols){
		return false;
	}	
	
	yamlData.resize(rows);
	
	for (int i = 0; i < yamlData.size(); i++){
		yamlData[i].resize(cols);
	}

	for (int i = 0; i < data.size(); i++){
		int a = i / cols;
    int b = i % cols;
		yamlData[a][b] = data[i];
	}	
	
	
	return true;
}

void YamlData::printYamlData(){
	
	printf("rows: %d, cols: %d\n", rows, cols);
	
	for (int j = 0; j < cols; j++){
		for (int i = 0; i< rows; i++){
			printf("data[%d][%d]=%d,", i, j, yamlData[i][j]);
		}
	}
}
int YamlData::data(int i ,int j){
	return yamlData[i][j];
}
