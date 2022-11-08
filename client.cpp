#include <fstream>
#include <iostream>
#include <thread>
#include <sys/time.h>
#include <sys/wait.h>
#include <vector>

#include "BoundedBuffer.h"
#include "common.h"
#include "Histogram.h"
#include "HistogramCollection.h"
#include "FIFORequestChannel.h"

// ecgno to use for datamsgs
#define EGCNO 1

using namespace std;

// functionality of the patient threads
void patient_thread_function (int p_no, int n, BoundedBuffer* request_buffer) {
    datamsg x(p_no, 0.0, EGCNO);

    for (int i = 0; i < n; i++) {
        request_buffer->push((char *) &x, sizeof(datamsg));
        x.seconds += 0.004;
    }
}

// functionality of the file thread
void file_thread_function (int m, string f, FIFORequestChannel* chan, BoundedBuffer* request_buffer) {
    //gathering the file size
    filemsg fm(0,0);
    string output_file = "received/"+f;
    int64_t file_size;
    int len = sizeof(filemsg) + f.size() + 1;
    char* buf = new char[len];
    memcpy(buf, &fm, sizeof(filemsg));
    strcpy(buf + sizeof(filemsg), f.c_str());
    chan->cwrite(buf, len);
    chan->cread(&file_size, sizeof(int64_t));

    //allocating memory to the file
    FILE* request_file = fopen(output_file.c_str(), "wb");
    fseek(request_file, file_size, SEEK_SET);
    fclose(request_file);

    //pushing incremements of m to the buffer
    for (int i = 0; i <= file_size/m; i++) {
        int file_left = file_size - m*i;
        filemsg* requested_file = (filemsg*) buf;
        requested_file->offset = m*i;
        if (file_left < m) {
            requested_file -> length = file_left;
        }
        else {
            requested_file -> length = m;
        }
        request_buffer->push(buf, len);
    }

    delete[] buf;
}

// functionality of the worker threads
void worker_thread_function (int _m, string f, FIFORequestChannel* worker_chan, BoundedBuffer* request_buffer, BoundedBuffer* response_buffer) {
    char buf[MAX_MESSAGE];
    double data_response;

    while(true) { //forever loop
        request_buffer->pop(buf, sizeof(buf));
        MESSAGE_TYPE* m = (MESSAGE_TYPE *) buf;

        if (*m == DATA_MSG) { //getting EGCNO value
            worker_chan->cwrite(buf, sizeof(datamsg));
            worker_chan->cread(&data_response, sizeof(double));
            pair<int, double> p;
            p.first = ((datamsg *) buf)->person;
            p.second = data_response;
            response_buffer->push((char *) &p, sizeof(p));
        }
        else if (*m == FILE_MSG) { 
            //gathering the buf to write to the file
            string output_file = "received/"+f;
            filemsg fm = *(filemsg *) buf;
            char* buf2 = new char [_m];
            int len = sizeof(filemsg) + f.size() + 1;
            worker_chan->cwrite(buf, len);
            worker_chan->cread(buf2, fm.length);

            //writing to the file
            FILE* request_file = fopen(output_file.c_str(), "r+b");
            fseek(request_file, fm.offset, SEEK_SET);
            fwrite(buf2, sizeof(char), fm.length, request_file);
            fclose(request_file);

            delete[] buf2;
        }
        else if (*m == QUIT_MSG) { //break out of loop and delete the server
            worker_chan->cwrite(m, sizeof(MESSAGE_TYPE));
            delete worker_chan;
            break;
        }
    }
}

// functionality of the histogram threads
void histogram_thread_function (HistogramCollection* hc, BoundedBuffer* response_buffer) {
    pair<int, double> p; 

    while(true) { //forever loop
        response_buffer->pop((char *) &p, sizeof(p));
        if (p.first == -1) {
            break;
        }
        hc->update(p.first, p.second);
    }
}


int main (int argc, char* argv[]) {
    int n = 1000;	// default number of requests per "patient"
    int p = 10;		// number of patients [1,15]
    int w = 100;	// default number of worker threads
	int h = 20;		// default number of histogram threads
    int b = 20;		// default capacity of the request buffer (should be changed)
	int m = MAX_MESSAGE;	// default capacity of the message buffer
	string f = "";	// name of file to be transferred
    
    // read arguments
    int opt;
	while ((opt = getopt(argc, argv, "n:p:w:h:b:m:f:")) != -1) {
		switch (opt) {
			case 'n':
				n = atoi(optarg);
                break;
			case 'p':
				p = atoi(optarg);
                break;
			case 'w':
				w = atoi(optarg);
                break;
			case 'h':
				h = atoi(optarg);
				break;
			case 'b':
				b = atoi(optarg);
                break;
			case 'm':
				m = atoi(optarg);
                break;
			case 'f':
				f = optarg;
                break;
		}
	}
    
	// fork and exec the server
    int pid = fork();
    if (pid == 0) {
        execl("./server", "./server", "-m", (char*) to_string(m).c_str(), nullptr);
    }
    
	// initialize overhead (including the control channel)
	FIFORequestChannel* chan = new FIFORequestChannel("control", FIFORequestChannel::CLIENT_SIDE);
    BoundedBuffer request_buffer(b);
    BoundedBuffer response_buffer(b);
	HistogramCollection hc;

    //initializing thead and channel vectors
    vector<thread> producer_threads; 
    vector<thread> worker_threads;
    vector<thread> histogram_threads;
    vector<FIFORequestChannel*> worker_chans;

    // making histograms and adding to collection
    for (int i = 0; i < p; i++) {
        Histogram* h = new Histogram(10, -2.0, 2.0);
        hc.add(h);
    }
	
	// record start time
    struct timeval start, end;
    gettimeofday(&start, 0);

    /* create all threads here */
    for (int i = 0; i < w; i++) { //creating w worker threads and channels
        MESSAGE_TYPE new_chan = NEWCHANNEL_MSG;
        char buf[MAX_MESSAGE];
        chan->cwrite(&new_chan, sizeof(MESSAGE_TYPE));
        chan->cread(buf, sizeof(datamsg));
        string channel_name(buf);
        worker_chans.push_back(new FIFORequestChannel(channel_name, FIFORequestChannel::CLIENT_SIDE));
        worker_threads.push_back(thread(worker_thread_function, m, f, worker_chans[i], &request_buffer, &response_buffer));
    }

    if (f == "") { //no file transfer 
        for (int i = 0; i < p; i++) { //creating p patient threads
            producer_threads.push_back(thread(patient_thread_function, i+1, n, &request_buffer));
        }
    }
    else { //file transfer requested, creating one file thread, and obtaining the file name and size
        producer_threads.push_back(thread(file_thread_function, m, f, chan, &request_buffer));
    }

    if (f == "") { //no file transfer
        for (int i = 0; i < h; i++) { //creating h histogram threads
            histogram_threads.push_back(thread(histogram_thread_function, &hc, &response_buffer));
        }
    }

	/* join all threads here */
    //joining all producer threads
    if (f == "") { //no file transfer
        for (int i = 0; i < p; i++) {
            producer_threads.at(i).join();
        }
    }
    else { //file transfer requested
        producer_threads.at(0).join();
    }

    //sending quit mesage to worker threads
    MESSAGE_TYPE q = QUIT_MSG;
    for (int i = 0; i < w; i++) {
        request_buffer.push((char *) &q, sizeof(MESSAGE_TYPE));
    }
    //joining all worker threads
    for (int i = 0; i < w; i++) {
        worker_threads.at(i).join();
    }

    //sending quit message to histogram threads and joining them
    pair<int, double> _pair;
    _pair.first = -1;
    _pair.second = -1.0;
    if (f == "") {
        for (int i = 0; i < h; i++) {
            response_buffer.push((char *) &_pair, sizeof(_pair));
        }
        for (int i = 0; i < h; i++) {
            histogram_threads.at(i).join();
        }
    }

	// record end time
    gettimeofday(&end, 0);

    // print the results
	if (f == "") {
		hc.print();
	}
    int secs = ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) / ((int) 1e6);
    int usecs = (int) ((1e6*end.tv_sec - 1e6*start.tv_sec) + (end.tv_usec - start.tv_usec)) % ((int) 1e6);
    cout << "Took " << secs << " seconds and " << usecs << " micro seconds" << endl;

	// quit and close control channel and worker channels
    chan->cwrite ((char *) &q, sizeof (MESSAGE_TYPE));
    cout << "All Done!" << endl;
    delete chan;

	// wait for server to exit
	wait(nullptr);
}
