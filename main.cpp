#include <iostream>
#include <string.h>
#include <fstream>
#include <vector>
#include <thread>
#include <future>
#include <functional>
#include <algorithm>
#include <mutex>

#include <list>

#include "version.h"

using namespace std;

using values_t = vector<string>;

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
        cout << "container size = " << vct.size() << ", front = " << vct.front() << ", \t\tback = " << vct.back() << endl;
    }
}

template <typename T>
class ReduceContainer
{
    vector<T> values;
    //mutex m;
public:
    ReduceContainer() {}
    ReduceContainer(const ReduceContainer &rc) : values(rc.values)/*, m(rc.m) */{}
    ReduceContainer(ReduceContainer &&rc)
    {
        values = move(rc.values);
    }
    ~ReduceContainer() {}

    void insert(T& val)
    {
        //unique_lock<mutex> lk(m);
        auto it = lower_bound(values.begin(), values.end(), val);
        values.insert(it, val);
    }

    vector<T> &get_values()
    {
        return values;
    }
};

void shuffle_thread(int rnum_, const values_t &strings_, vector<ReduceContainer<string>> &rcont_)
{
    for(auto s : strings_)
    {
        int rcont_id = hash<string>{}(s) % rnum_;
        rcont_[rcont_id].insert(s);
    }
}

void do_shuffle(int mnum_, int rnum_, const values_t &strings_, vector<ReduceContainer<string>> &rcont_)
{
    vector<thread> shuffle_threads;

    auto t = thread(shuffle_thread, rnum_, strings_[0], rcont_);
//    for(auto i = 0; i < mnum_; ++i)
//    {
//        shuffle_threads.push_back(thread(shuffle_thread, std::ref(rnum_), std::ref(strings_[i]), std::ref(rcont_)));
//    }

//    for(auto &t : shuffle_threads)
//    {
//        t.join();
//    }

//    for(auto rc : rcont_)
//    {
//        cout << "container size = " << rc.get_values().size()
//             << ", front = " << rc.get_values().front()
//             << ", \t\tback = " << rc.get_values().back() << endl;
//    }
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
            vector<values_t> input_strings;
            do_map(mnum, path, slices, input_strings);

            //Shuffle
            vector<ReduceContainer<string>> shuffled_strings(rnum);
            cout << shuffled_strings.size() << shuffled_strings[0].get_values().size() << endl;
            shuffle_thread(rnum, input_strings[0], shuffled_strings);

            //Reduce
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }
    return 0;
}
