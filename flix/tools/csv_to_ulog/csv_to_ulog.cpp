// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// 将CSV日志文件转换为ULog格式的工具

#include <ulog_cpp/simple_writer.hpp>
#include <rapidcsv.h>
#include <vector>
#include <string>
#include <filesystem>

using std::vector;
using std::string;

struct Data {
	uint64_t timestamp;
	float values[30];
};

int main(int argc, char** argv)
{
	if (argc < 2) {
		printf("用法：%s file.csv [file.ulg]\n", argv[0]);
		return -1;
	}

	// 检查输入文件是否存在
	if (!std::filesystem::exists(argv[1])) {
		printf("输入文件 \"%s\" 不存在\n", argv[1]);
		return -1;
	}

	// 打开CSV文件
	rapidcsv::Document csv(argv[1]);
	auto columns = csv.GetColumnNames();


	// 打开ULog文件
	string ulog_file;
	if (argc < 3) {
		ulog_file = std::filesystem::path(argv[1]).replace_extension(".ulg").string();
	} else {
		ulog_file = argv[2];
	}
	ulog_cpp::SimpleWriter writer(ulog_file.c_str(), 0);
	writer.writeInfo("sys_name", "flix");

	vector<ulog_cpp::Field> fields;
	fields.push_back(ulog_cpp::Field("uint64_t", "timestamp"));
	columns.erase(columns.begin()); // 移除时间戳列
	for (auto& column : columns) {
		// ULog的有效字段名：[a-z0-9_]+
		std::replace(column.begin(), column.end(), '.', '_'); // 将点替换为下划线
		std::transform(column.begin(), column.end(), column.begin(), [](unsigned char c) { return std::tolower(c); }); // 将列名转换为小写
		fields.push_back(ulog_cpp::Field("float", column));
	}

	const char* msg_name = "state";
	writer.writeMessageFormat(msg_name, fields);
	writer.headerComplete();

	const uint16_t msg_id = writer.writeAddLoggedMessage(msg_name);

	for (size_t i = 0; i < csv.GetRowCount(); i++) {
		Data data;
		data.timestamp = csv.GetCell<float>(0, i) * 1000000.0;
		for (size_t j = 1; j <= columns.size(); j++) {
			data.values[j - 1] = csv.GetCell<float>(j, i);
		}
		writer.writeData(msg_id, data);
	}
}
