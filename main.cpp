#include <iostream>
#include <string.h>
#include <fstream>
#include <vector>
#include <thread>
#include <functional>
#include <algorithm>
#include <mutex>
#include <tuple>
#include <memory>

#include "version.h"

using namespace std;

using values_t = vector<string>;
using reducer_data_t = tuple<unique_ptr<mutex>, values_t, thread>;

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

    offsets.reserve(slices_count);

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

auto mapper = [] (string &line, values_t &strings)
{
    strings.push_back(line);
};

void mapper_thread(const std::string &path_, const slice &slice_, values_t &strings, std::function<void(string &, values_t &)> m)
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

void do_map(int mnum_, const std::string &path_, const vector<slice> &slices_, vector<values_t> &input_strings_)
{
    input_strings_.reserve(mnum_);
    vector<thread> mapper_threads;

    for(auto i = 0; i < mnum_; ++i)
    {
        input_strings_.push_back(values_t());
        mapper_threads.push_back(thread(mapper_thread, std::ref(path_), std::ref(slices_[i]), std::ref(input_strings_.back()), mapper));
    }

    for(auto &t : mapper_threads)
    {
        t.join();
    }

    for(auto vct : input_strings_)
    {
        cout << "container size = " << vct.size()
             << ", front =" << vct.front()
             << ", \t\tback =" << vct.back() << endl;
    }
}


void shuffle_thread(int rnum_, const values_t &strings_, vector<reducer_data_t> &rdata_)
{
    for(auto s : strings_)
    {
        int rcont_id = hash<string>{}(s) % rnum_;
        //rdata_[rcont_id].insert(s);
    }
}

void do_shuffle(int mnum_, int rnum_, const vector<values_t> &strings_, vector<reducer_data_t> &rdata_)
{
    vector<thread> shuffle_threads;

    rdata_.resize(rnum_);
    for(auto &r : rdata_)
    {
        get<0>(r) = move(make_unique<mutex>());
    }

    for(auto i = 0; i < mnum_; ++i)
    {
        const values_t &values = strings_[i];
        shuffle_threads.push_back(
                    thread([rnum_, values, &rdata_]
                            {
                                for(auto s : values)
                                {
                                    int r_id = hash<string>{}(s) % rnum_;
                                    auto &m = get<0>(rdata_[r_id]);
                                    auto &strs = get<1>(rdata_[r_id]);

                                    lock_guard<mutex> lk(*m);
                                    auto it = lower_bound(strs.begin(), strs.end(), s);
                                    strs.insert(it, s);
                                }
                            }
                           )
                    );
    }

    for(auto &t : shuffle_threads)
    {
        t.join();
    }

    for(auto &rd : rdata_)
    {
        auto &values = get<1>(rd);
        cout << "container size = " << values.size()
             << ", front =" << values.front()
             << ", \t\tback =" << values.back() << endl;
    }
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

            //Map
            cout << "Map ->" << endl;
            vector<values_t> input_strings;
            do_map(mnum, path, slices, input_strings);

            //Shuffle
            cout << "Shuffle ->" << endl;
            vector<reducer_data_t> reducers_data;
            do_shuffle(mnum, rnum, input_strings, reducers_data);

            //Reduce
            cout << "Reduce ->" << endl;
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
