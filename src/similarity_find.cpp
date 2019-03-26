//============================================================================
// Name        : similarity_find.cpp
// Author      : Peter
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <cstdlib>
#include <regex>
#include <map>
using namespace std;

#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/scope_exit.hpp>

#ifndef MYSQLPP_MYSQL_HEADERS_BURIED
#	define MYSQLPP_MYSQL_HEADERS_BURIED
#endif

#include <mysql++/mysql++.h>

boost::filesystem::path working_dir = "/home/peter/similarity_find/";

mysqlpp::Connection conn(false);

std::map<int, std::string> problems;
std::map<int, std::tuple<std::string, std::string>> users;
std::map<int, int> solution_authors;

void f(int s_id, int u_id, int p_id)
{
	boost::filesystem::path problem_dir = working_dir / to_string(p_id);
	int per;
	int s_id2;
	{
		std::ifstream fin((problem_dir / (std::to_string(s_id) + ".sim")).string());

		std::string line;
		while (std::getline(fin, line)) {
			{
				std::smatch m;
				static std::regex reg(R"((\d{1,3})\s%)");
				std::regex_search(line, m, reg);
				if (m.size() != 2) {
					continue;
				}
				per = std::stoi(m[1]);
			}

//			if (per < 40) {
//				continue;
//			}

			{
				std::string line2 = line;
				std::smatch m;
				static std::regex reg(R"((\d{0,6})\.cpp\smaterial)");
				std::regex_search(line2, m, reg);
				if (m.size() != 2) {
					continue;
				}
				s_id2 = std::stoi(m[1]);
			}

			//学号1, 姓名1, 提交号1, 学号2, 姓名2, 提交号2, 题目编号, 题目名称, 百分比
			boost::format format("%s | %s | %d | %s | %s | %d | %s | %s | %d");

			try {
				format % get<0>(users.at(u_id)) % get<1>(users.at(u_id)) % s_id;
			} catch (const std::out_of_range & e) {
				cerr << u_id << endl;
				cerr << __LINE__ << "  " << e.what() << endl;
				throw;
			}

			int u_id2 = solution_authors.at(s_id2);

			if (u_id == u_id2) {
				continue;
			}

			try {
				format % get<0>(users.at(u_id2)) % get<1>(users.at(u_id2)) % s_id2;
			} catch (const std::out_of_range & e) {
				cerr << __LINE__ << "  " << e.what() << endl;
				throw;
			}

			try {
				format % p_id % problems.at(p_id);
			} catch (const std::out_of_range & e) {
				cerr << __LINE__ << "  " << e.what() << endl;
				throw;
			}

			format % per;

			cout << format.str() << endl;
		}
	}
}

int main()
{
	boost::filesystem::remove_all(working_dir);
	boost::filesystem::create_directories(working_dir);

	conn.set_option(new mysqlpp::SetCharsetNameOption("utf8"));
	conn.connect("ojv4", "localhost", "root", "123");

	int c_id = 29;

	{
		mysqlpp::Query query = conn.query(
				"select u_id, u_name, u_realname from user "
				"where u_id in (select u_id from course_user where c_id = %0)"
		);
		query.parse();
		auto res = query.store(c_id);
		if (query.errnum()) {
			cerr << __LINE__ << "  " << query.errnum() << "   " << query.error() << endl;
			throw 0;
		}
		for(const mysqlpp::Row & row : res) {
			int u_id = row["u_id"];
			users[u_id] = {string(row["u_name"].c_str()), string(row["u_realname"].c_str())};
		}
	}
	cout << "users" << endl;
	{
		mysqlpp::Query query = conn.query(
				"select p_id, p_title from problem "
				"where p_id in (select p_id from course_problem where c_id = %0)"
		);
		query.parse();
		auto res = query.store(c_id);
		if (query.errnum()) {
			cerr << __LINE__ << "  " << query.errnum() << "   " << query.error() << endl;
			throw 0;
		}
		for(const mysqlpp::Row & row : res) {
			int p_id = row["p_id"];
			problems[p_id] = row["p_title"].c_str();
		}
	}
	cout << "problems" << endl;

	mysqlpp::Query query = conn.query(
			"select solution.s_id, u_id, p_id, source_code "
			"from solution, source "
			"where solution.s_id = source.s_id and c_id = %0 and s_lang in (0, 1) "
//			"limit 1000"
	);
	query.parse();

	mysqlpp::UseQueryResult use = query.use(c_id);

	if (!use) {
		cerr << __LINE__ << "  " << query.errnum() << "   " << query.error() << endl;
	}

	while (auto solution = use.fetch_row()) {
		std::string p_id {solution["p_id"].c_str()};
		std::string u_id {solution["u_id"].c_str()};
		std::string s_id {solution["s_id"].c_str()};

		cerr << s_id << endl;

		solution_authors[stoi(s_id)] = stoi(u_id);

		boost::filesystem::path problem_dir = working_dir / p_id;
		boost::filesystem::create_directories(problem_dir);

		std::string source_file = (problem_dir / s_id).string() + ".cpp";

		{
			std::ofstream fout(source_file);
			fout << solution["source_code"];
		}


		std::string command = (boost::format("sim_c++ -ep %1% / %2%/* > %2%/%3%.sim") % source_file % problem_dir.string() % s_id).str();
		std::system(command.c_str());

//		namespace bp = boost::process;
//		std::string command = (boost::format("sim_c++ -ep %1% / %2%/*") % source_file % problem_dir.string()).str();
//		bp::system(command, bp::std_out > "sim2333.sim");

		try {
			f(stoi(s_id), stoi(u_id), stoi(p_id));
		} catch (const std::out_of_range & e) {
//			cout << __LINE__ << "  " << e.what() << endl;
//			throw;
		}
	}

	return 0;
}
