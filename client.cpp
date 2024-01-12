#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>

#include "mpi.h"
#include "tema3.h"

using namespace std;

ClientData *read_file(int rank) {
  string filename = "in" + to_string(rank) + ".txt";
  printf("Client %d: reading file %s\n", rank, filename.c_str());
  ClientData *client_data = new ClientData();

  ifstream infile(filename);
  int file_count;
  infile >> file_count;
  for (int i = 0; i < file_count; ++i) {
    string filename;
    int chunk_count;
    infile >> filename >> chunk_count;
    for (int j = 0; j < chunk_count; ++j) {
      string chunk;
      infile >> chunk;
      client_data->owned_files[filename].push_back(chunk);
    }
  }
  int wished_file_count;
  infile >> wished_file_count;
  for (int i = 0; i < wished_file_count; ++i) {
    string filename;
    infile >> filename;
    client_data->wished_files.push_back(filename);
  }
  infile.close();

  return client_data;
}

void print_client_data(ClientData *client_data, FILE *logfile) {
  fprintf(stdout, "Owned files:\n");
  for (auto it = client_data->owned_files.begin();
       it != client_data->owned_files.end(); ++it) {
    fprintf(stdout, "%s: ", it->first.c_str());
    for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
      fprintf(stdout, "%lu %s\n", it2->size(), it2->c_str());
    }
    fprintf(stdout, "\n");
  }
  // fprintf(stdout, "Wished files:\n");
  for (auto it = client_data->wished_files.begin();
       it != client_data->wished_files.end(); ++it) {
    fprintf(stdout, "%s ", it->c_str());
  }
  fprintf(stdout, "\n");
}

int send_seed_info_to_tracker(ClientData *client_data, int rank) {
  // hello
  int no_files = client_data->owned_files.size();
  MPI_Send(&no_files, 1, MPI_INT, TRACKER_RANK, HELLO, MPI_COMM_WORLD);

  // send file names and chunks to tracker
  for (auto it = client_data->owned_files.begin();
       it != client_data->owned_files.end(); ++it) {
    string filename = it->first;
    int chunk_count = it->second.size();
    FileHeader file_header;
    strcpy(file_header.filename, filename.c_str());
    file_header.no_chunks = chunk_count;
    MPI_Send(&file_header, sizeof(FileHeader), MPI_BYTE, TRACKER_RANK,
             FILE_INFO, MPI_COMM_WORLD);
    char *buffer = new char[(HASH_SIZE) * chunk_count];
    pack_data(buffer, it->second, HASH_SIZE);
    MPI_Send(buffer, (HASH_SIZE) * chunk_count, MPI_BYTE, TRACKER_RANK,
             CHUNK_INFO, MPI_COMM_WORLD);
  }
  // fprintf(stdout, "Client %d: sent seed info to tracker\n", rank);

  return 0;
}

int wait_for_tracker(int rank) {
  // fprintf(stdout, "Client %d: waiting for tracker\n", rank);
  bool tracker_ok;
  MPI_Bcast(&tracker_ok, 1, MPI_C_BOOL, TRACKER_RANK, MPI_COMM_WORLD);
  if (!tracker_ok) {
    fprintf(stderr, "Client %d: tracker ok error\n", rank);
    return 1;
  }
  // fprintf(stdout, "Client %d: tracker ok : %d \n", rank, tracker_ok);
  return 0;
}

int get_files_info(ClientArgs *par) {
  MPI_Status tracker_status;
  tracker_status.MPI_ERROR = MPI_SUCCESS;

  int no_files = par->client_data->wished_files.size();
  char *buffer = new char[no_files * (MAX_FILENAME)];
  pack_data(buffer, par->client_data->wished_files, MAX_FILENAME);
  MPI_Send(&no_files, 1, MPI_INT, TRACKER_RANK, REQUEST, MPI_COMM_WORLD);

  if (no_files == 0) {
    return 0;
  }

  // SEND FILENAMES

  MPI_Send(buffer, no_files * (MAX_FILENAME), MPI_BYTE, TRACKER_RANK, FILE_INFO,
           MPI_COMM_WORLD);

  // delete[] buffer;

  for (int i = 0; i < no_files; i++) {
    FileInfo *info = new FileInfo();
    File_Metadata metadata;
    MPI_Recv(&metadata, sizeof(File_Metadata), MPI_BYTE, TRACKER_RANK,
             FILE_INFO, MPI_COMM_WORLD, &tracker_status);
    strcpy(info->filename, par->client_data->wished_files[i].c_str());

    char *buffer = new char[metadata.total_size];
    MPI_Recv(buffer, metadata.total_size, MPI_BYTE, TRACKER_RANK, FILE_INFO,
             MPI_COMM_WORLD, &tracker_status);

    unpack_file(info, &metadata, buffer, &(metadata.total_size));

    par->tracker_data.push_back(info);
    // delete[] buffer;
    // printf("Client %d: received file info from tracker\n", par->rank);
  }

  printf("Client %d is done receiving file info from tracker\n", par->rank);

  return 0;
}

int seed_chunk(ClientArgs *args, MPI_Status *status) {
  MPI_Status seeder_status;
  Chunk_Metadata chunk_metadata;
  MPI_Recv(&chunk_metadata, sizeof(Chunk_Metadata), MPI_BYTE, status->MPI_SOURCE,
           CHUNK_INFO, MPI_COMM_WORLD, &seeder_status);
 char *chunk = new char[HASH_SIZE]; 
  for (auto file : args->client_data->owned_files) {
    if (strcmp(file.first.c_str(), chunk_metadata.filename) == 0) {
      memcpy(chunk, file.second[chunk_metadata.idx].c_str(), HASH_SIZE);
      // printf("Sa cada baietii %d %d|| %s\n", args->rank, chunk_metadata.idx, chunk);
      break;
    }
  }


    MPI_Send(chunk, HASH_SIZE, MPI_BYTE, seeder_status.MPI_SOURCE, FILE_INFO,
           MPI_COMM_WORLD);


  return 0;
}

int download_chunk(ClientArgs *args, int seeder_rank, int file_idx,
                   int chunk_idx) {

  Chunk_Metadata chunk_info;
  char *chunk = new char[HASH_SIZE];
  MPI_Status status;
  status.MPI_ERROR = MPI_SUCCESS;
  strcpy(chunk_info.filename,
         args->client_data->wished_files[file_idx].c_str());
  chunk_info.idx = chunk_idx;
  // printf("Sa cada toti baietii %d %d|| %s\n", args->rank, chunk_idx, chunk_info.filename);
  int data = 0;
  MPI_Ssend(&data, 1, MPI_INT, seeder_rank, PEER_REQUEST, MPI_COMM_WORLD);
  MPI_Ssend(&chunk_info, sizeof(Chunk_Metadata), MPI_BYTE, seeder_rank,
           CHUNK_INFO, MPI_COMM_WORLD);

  MPI_Recv(chunk, HASH_SIZE, MPI_BYTE, seeder_rank, FILE_INFO,
           MPI_COMM_WORLD, &status);
  // printf("C%d|S%d<%s> Saved chunk %d> %s\n", args->rank,seeder_rank,
  //          args->client_data->wished_files[file_idx].c_str(), chunk_idx,
  //          chunk);

  args->client_data->owned_files[args->client_data->wished_files[file_idx]]
      .push_back(string(chunk, HASH_SIZE));

  return 0;
}

int signal_file_download_finished(ClientArgs *args, int file_idx) {
  int data = 0;
  MPI_Send(&data, 1, MPI_INT, TRACKER_RANK, FINISHED_FILE_DOWNLOAD,
           MPI_COMM_WORLD);
  char filename[MAX_FILENAME];
  strcpy(filename, args->client_data->wished_files[file_idx].c_str());
  MPI_Send(filename, MAX_FILENAME, MPI_BYTE, TRACKER_RANK,
           FINISHED_FILE_DOWNLOAD, MPI_COMM_WORLD);

  return 0;
}

int signal_all_downloads_finished() {
  int data = 0;
  MPI_Send(&data, 1, MPI_INT, TRACKER_RANK, FINISHED_ALL_DOWNLOADS,
           MPI_COMM_WORLD);
  return 0;
}

int update_tracker(ClientArgs *args,  int file_idx, int chunk_idx) {
  int data = 0; 
  // printf("C%d> Updating tracker with %d to chid\n", args->rank, chunk_idx);
  MPI_Send(&data, 1, MPI_INT, TRACKER_RANK, CLIENT_UPDATE, MPI_COMM_WORLD);
  Chunk_Metadata chunk_metadata;
  strcpy(chunk_metadata.filename,
         args->client_data->wished_files[file_idx].c_str());
  chunk_metadata.idx = chunk_idx;

  MPI_Send(&chunk_metadata, sizeof(Chunk_Metadata), MPI_BYTE, TRACKER_RANK,
           CLIENT_UPDATE, MPI_COMM_WORLD);

  MPI_Status status;

  File_Metadata file_metadata;
  MPI_Recv(&file_metadata, sizeof(File_Metadata), MPI_BYTE, TRACKER_RANK,
           FILE_INFO, MPI_COMM_WORLD, &status);

  char *buffer = new char[file_metadata.total_size];
  MPI_Recv(buffer, file_metadata.total_size, MPI_BYTE, TRACKER_RANK, FILE_INFO,
           MPI_COMM_WORLD, &status);

  FileInfo *file = args->tracker_data[file_idx];
  int offset = 0;

  file->seeders = vector<Seeder>(file_metadata.no_seeders);
  for (int i = 0; i < file_metadata.no_seeders; i++) {
    memcpy(&file->seeders[i], buffer + i * sizeof(Seeder), sizeof(Seeder));
    offset += sizeof(Seeder);
  }

  unpack_data_int(buffer + offset, file->leechers, file_metadata.no_leechers);
 
  return 0;
}

void save_file(string filename, ClientArgs *args) {
  string out_filename = "client" + to_string(args->rank) + "_" + filename;
  ofstream outfile(out_filename);
  size_t total_chunks = args->client_data->owned_files[filename].size();
  for (size_t i = 0; i < total_chunks; ++i) {
        outfile << args->client_data->owned_files[filename][i];
        
        // Add newline if it's not the last chunk
        if (i < total_chunks - 1) {
            outfile << endl;
        }

        // Display information about saved chunk
        printf("Saved chunk %d %s\n", i, args->client_data->owned_files[filename][i].c_str());
    }

    outfile.close();
    // printf("C%d> Saved file %s\n", args->rank, out_filename.c_str());
}

int update_file_info(ClientArgs *args, FileInfo *updated_file, int file_idx) {

  
  // FileInfo *file = args->tracker_data[file_idx];
  args->tracker_data[file_idx]->seeders = updated_file->seeders;
  args->tracker_data[file_idx]->leechers = updated_file->leechers;
  // printf("C%d> Updated file info for %s\n", args->rank, file->filename);

  return 0;
}


int handle_tracker_message(ClientArgs *par, int flag, MPI_Status *status) {

if (flag == ALL_CLIENTS_DONE) {
    par->active = false;
  }
else {
  fprintf(stderr, "Peer %d: received unknown %d flag %d with tag %d from tracker\n", par->rank, status->MPI_SOURCE,flag, status->MPI_TAG);
}
  return 0;
}
