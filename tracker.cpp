#include <algorithm>
#include <fstream>
#include <iostream>
#include <numeric>

#include "mpi.h"
#include "tema3.h"

using namespace std;

int bootstrap_tracker(Tracker *tracker_data, int rank, int numtasks) {
  int connected_clients = 0;
  MPI_Status *client_status = new MPI_Status();

  while (connected_clients + 1 < numtasks) {
    int no_files;
    ClientInfo *client_info = new ClientInfo();
    MPI_Recv(&no_files, 1, MPI_INT, MPI_ANY_SOURCE, HELLO, MPI_COMM_WORLD,
             client_status);
    // fprintf(stdout, "Tracker: received hello %d from client %d\n", no_files,
    // client_status->MPI_SOURCE);

    client_info->id = client_status->MPI_SOURCE;
    client_info->status = true;
    tracker_data->clients.insert({client_info->id, client_info});

    for (int i = 0; i < no_files; i++) {
      FileInfo *file_info = new FileInfo();
      FileHeader file_header;
      MPI_Recv(&file_header, sizeof(FileHeader), MPI_BYTE,
               client_status->MPI_SOURCE, FILE_INFO, MPI_COMM_WORLD,
               client_status);
      // fprintf(stdout, "Tracker: received file info %s from client %d\n",
      // file_header.filename, client_status->MPI_SOURCE);
      strcpy(file_info->filename, file_header.filename);

      file_info->seeders.push_back(
          Seeder{client_info->id, file_header.no_chunks});

      char *buffer = new char[(HASH_SIZE) * file_header.no_chunks];
      MPI_Recv(buffer, (HASH_SIZE) * file_header.no_chunks, MPI_BYTE,
               client_status->MPI_SOURCE, CHUNK_INFO, MPI_COMM_WORLD,
               client_status);
      unpack_data(buffer, file_info->chunks, file_header.no_chunks, HASH_SIZE);
      tracker_data->files.insert({file_info->filename, file_info});

      tracker_data->clients[client_info->id]->owned_files.push_back(
          file_info->filename);
      delete[] buffer;
    }
    tracker_data->clients.insert({client_info->id, client_info});

    connected_clients++;
  }

  bool tracker_ok = true;

  MPI_Bcast(&tracker_ok, 1, MPI_C_BOOL, TRACKER_RANK, MPI_COMM_WORLD);

  // delete client_status;

  return 0;
}

int send_files_info(Tracker *tracker, int client_rank, int no_files) {


  if (no_files == 0) {
    tracker->clients[client_rank]->status = false;
    return 0;
  }




  vector<string> files;
  vector<FileInfo *> requested_files;
  vector<int> new_seeders;

  // get the list of files that the client wishes to download
  char *buffer = new char[(MAX_FILENAME)*no_files];
  MPI_Status status;
  status.MPI_ERROR = MPI_SUCCESS;
  MPI_Recv(buffer, no_files * (MAX_FILENAME), MPI_BYTE, client_rank, FILE_INFO,
           MPI_COMM_WORLD, &status);


  printf("Primit %d fisiere de la %d\n", no_files, client_rank);


  if (status.MPI_ERROR != MPI_SUCCESS) {
    fprintf(stderr, "Tracker: error in receiving file info from client\n");
    return 1;
  }
  unpack_variable_data(buffer, files, no_files, MAX_FILENAME);

  // search for the files in the tracker's database
  for (int i = 0; i < (int)files.size(); i++) {
    // printf("File %s %lu\n", files[i].c_str(), files[i].size());
    if (tracker->files.find(files[i]) != tracker->files.end()) {
      requested_files.push_back(tracker->files[files[i]]);
      tracker->files[files[i]]->leechers.push_back(client_rank);
    }
  }

  // delete previous buffer allocation
  // delete[] buffer;

  // printf("HMHM %d %lu\n", no_files, requested_files.size());

  for (int i = 0; i < (int)requested_files.size(); i++) {
    // send the file metadata for every requested file
    int size = (HASH_SIZE)*requested_files[i]->chunks.size() +
               requested_files[i]->seeders.size() * sizeof(Seeder) +
               requested_files[i]->leechers.size() * sizeof(int);

    File_Metadata requested_file_data = {
        static_cast<int>(requested_files[i]->chunks.size()),
        static_cast<int>(requested_files[i]->seeders.size()),
        static_cast<int>(requested_files[i]->leechers.size()), size};

    MPI_Send(&requested_file_data, sizeof(File_Metadata), MPI_BYTE, client_rank,
             FILE_INFO, MPI_COMM_WORLD);
    // send the header for every seeder
    printf("Moare aici %d\n", client_rank);

    char *temp_buffer = new char[size];
    pack_file(requested_files[i], temp_buffer, &size);
    printf("Uau %d\n", client_rank);

    MPI_Send(temp_buffer, size, MPI_BYTE, client_rank, FILE_INFO, MPI_COMM_WORLD);
    // printf("Sa cada baietii %d\n", client_rank);

    // printf("Sent file %s to %d\n", requested_files[i]->filename, client_rank);
    // if (i + 1 < (int)requested_files.size()) {
    //   delete[] buffer;
    // }
    // printf("Done\n");
  }

  printf("IESE la rank %d???????????????????????\n", client_rank);

  return 0;
}

int handle_client_status_update(Tracker *tracker, MPI_Status *status) {
  Chunk_Metadata update;

  MPI_Recv(&update, sizeof(Chunk_Metadata), MPI_BYTE, status->MPI_SOURCE,
           CLIENT_UPDATE, MPI_COMM_WORLD, status);

  // printf("MA FUT IN MORTII MATII %d %s\n", update.idx, update.filename);


  bool found = false;
  for (auto seeder : tracker->files[update.filename]->seeders) {
    if (seeder.id == status->MPI_SOURCE) {
      // printf("Client %d has %d chunks from file %s >", status->MPI_SOURCE, update.idx + 1, update.filename);
      seeder.chunks_owned = update.idx + 1;
      // printf("Sa cada %d baieti\n", seeder.chunks_owned);
      found = true;
      break;
    }
  }
  if (!found)
    tracker->files[update.filename]->seeders.push_back(
        Seeder{status->MPI_SOURCE, update.idx + 1});




  // printf("Update %s seeders size: %lu\n", update.filename, tracker->files[update.filename]->seeders.size());

  // printf("Client %d has chunk %d from file %s\n", status->MPI_SOURCE, update.idx, update.filename);

  // FileInfo *file = tracker->files[update.filename];

  int size = tracker->files[update.filename]->seeders.size() * sizeof(Seeder) +
             tracker->files[update.filename]->leechers.size() * sizeof(int);
  File_Metadata requested_file_data = {static_cast<int>(tracker->files[update.filename]->chunks.size()),
                                       static_cast<int>(tracker->files[update.filename]->seeders.size()),
                                       static_cast<int>(tracker->files[update.filename]->leechers.size()),
                                       size};
   char *buffer = new char[size]; 

  MPI_Send(&requested_file_data, sizeof(File_Metadata), MPI_BYTE,
           status->MPI_SOURCE, FILE_INFO, MPI_COMM_WORLD);

  int offset = 0;
 for (int i = 0; i < (int)tracker->files[update.filename]->seeders.size(); i++) {
    memcpy(buffer + offset, &tracker->files[update.filename]->seeders[i],
           sizeof(Seeder));
    offset += sizeof(Seeder);
  }
 for (int i = 0; i < (int)tracker->files[update.filename]->leechers.size(); i++) {
    memcpy(buffer + offset, &tracker->files[update.filename]->leechers[i],
           sizeof(int));
    offset += sizeof(int);
  }
 if (offset != size) {
    fprintf(stderr, "Tracker: error in packing file info\n");
    return 1;
  }

  // printf("<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
  MPI_Send(buffer, size, MPI_BYTE, status->MPI_SOURCE, FILE_INFO,
           MPI_COMM_WORLD);
 //
  // printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n\n\n");

  return 0;
}

int handle_finished_file_download(Tracker *tracker, MPI_Status *status) {
  char *buffer = new char[MAX_FILENAME];
  MPI_Recv(buffer, MAX_FILENAME, MPI_BYTE, status->MPI_SOURCE,
           FINISHED_FILE_DOWNLOAD, MPI_COMM_WORLD, status);


  string filename(buffer);

  printf("Client %d finished downloading file %s\n", status->MPI_SOURCE,
         filename.c_str());

  for (auto seeder: tracker->files[filename]->seeders) {
    if (seeder.id == status->MPI_SOURCE) {
      seeder.chunks_owned = tracker->files[filename]->chunks.size();
      // printf("Sa cada %d baieti\n", seeder.chunks_owned);
      break;
    }
  }

  for (int i = 0; i < (int)tracker->files[filename]->leechers.size(); i++) {
    if (tracker->files[filename]->leechers[i] == status->MPI_SOURCE) {
      tracker->files[filename]->leechers.erase(
          tracker->files[filename]->leechers.begin() + i);
      break;
    }
  }

  // client owns all chunks

  // printf(">>>>>Client %d finished downloading file %s\n", status->MPI_SOURCE,
  //        filename.c_str());

  return 0;
}

int handle_all_client_downloads_finished(Tracker *tracker, MPI_Status *status) {
  printf("Client %d finished downloading all files\n", status->MPI_SOURCE);
  tracker->clients[status->MPI_SOURCE]->status = false;
  tracker->active = any_of(tracker->clients.begin(), tracker->clients.end(),
                           [](auto client) { return client.second->status; });
  // printf("Tracker active: %d\n", tracker->active);
  for (auto client : tracker->clients) {
    printf("Client %d status: %d\n", client.first, client.second->status);
  }

  return 0;
}

int signal_shutdown_to_clients(Tracker *tracker, int numtasks) {
  for (int i = 1; i < numtasks; i++) {
    int data = ALL_CLIENTS_DONE;
    MPI_Send(&data, 1, MPI_INT, i, PEER_REQUEST, MPI_COMM_WORLD);
    // MPI_Send(&data, 1, MPI_INT, i, ALL_CLIENTS_DONE, MPI_COMM_WORLD);
  }

  return 0;
}
