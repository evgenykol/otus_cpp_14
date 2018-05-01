#include <iostream>
#include <string.h>
#include <fstream>
#include <vector>
#include <thread>
#include <future>
#include <functional>
#include <algorithm>

#include "version.h"

using namespace std;

struct slice
{
    int begin_offset;
    int end_offset;
};

int check_console_input(const int mnum, const int rnum, const string &path)
{
    int result = 0;
    if (mnum < 0)
    {
        cout << "error, negative mnum : " << mnum << endl;
        result = 1;
    }

    if (rnum < 0)
    {
        cout << "error, negative rnum : " << rnum << endl;
        result = 1;
    }

    if(path.length() < 1)
    {
        cout << "error, bad path : " << path << endl;
        result = 1;
    }
    return result;
}

int split_file(const string &path, const int slices_count, vector<slice> &offsets)
{
    ifstream f(path, ios::binary | ios::ate);
    if(!f.is_open())
    {
        cout << "error, no such file: " << path << endl;
        return 1;
    }
    auto file_size = f.tellg();
    auto slice_size = file_size/slices_count;
    decltype(file_size) cur_offset = 0;

    f.seekg(cur_offset);
    while (cur_offset < file_size)
    {
        slice slc;
        slc.begin_offset = cur_offset;

        if ((file_size - cur_offset) > slice_size)
        {
            f.seekg(slice_size, ios_base::cur);
            while ((f.get() != '\n') && (f.peek() != EOF));
            cur_offset = f.tellg();
        }
        else
        {
            cur_offset = file_size;
        }

        slc.end_offset = cur_offset;
        --slc.end_offset;
        offsets.push_back(slc);
    }

    f.close();

    return 0;
}

auto mapper = [] (string &line, vector<string> &strings)
{
    strings.push_back(line);
};

void mapper_thread(const std::string &path_, const slice &slice_, vector<string> &strings, std::function<void(string &, vector<string> &)> m)
{
    ifstream f(path_, ios_base::in);
    if(!f.is_open())
    {
        cout << "mapper_thread error, can't open file. PID = " << this_thread::get_id() << endl;
    }

    string line;
    f.seekg(slice_.begin_offset);
    while (std::getline(f, line) && (f.tellg() <= slice_.end_offset))
    {
        m(line, strings);
    }

    std::sort(strings.begin(), strings.end());

    f.close();
}

int main(int argc, char* argv[])
{
    try
    {
        string path;
        int mnum = 0;
        int rnum = 0;

        if ((argc > 1) &&
                (!strncmp(argv[1], "-v", 2) || !strncmp(argv[1], "--version", 9)))
        {
            cout << "version " << version() << endl;
            return 0;
        }
        else if (argc == 4)
        {
            path = string(argv[1]);
            mnum = atoi(argv[2]);
            rnum = atoi(argv[3]);
            cout << "yamr M: " << mnum << ", R: " << rnum << ", path: " << path << endl;
        }
        else
        {
            std::cerr << "Usage: yamr <src> <mnum> <rnum>\n";
            return 1;
        }

        if(check_console_input(mnum, rnum, path))
        {
            return 1;
        }

        vector<slice> slices;
        if(!split_file(path, mnum, slices))
        {

            //Mapper
            vector<vector<string>> input_strings;
            input_strings.reserve(mnum);
            vector<thread> mapper_threads;

            for(auto i = 0; i < mnum; ++i)
            {
                input_strings.push_back(vector<string>());
                mapper_threads.push_back(thread(mapper_thread, std::ref(path), std::ref(slices[i]), std::ref(input_strings.back()), mapper));
            }

            for(auto &t : mapper_threads)
            {
                t.join();
            }

            for(auto vct : input_strings)
            {
                cout << "container size = " << vct.size() << ", front = " << vct.front() << ", back = " << vct.back() << endl;
            }
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
