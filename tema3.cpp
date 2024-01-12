#include <pthread.h>

#include <algorithm>
#include <cstddef>
#include <set>

#include "mpi.h"
#include "tema3.h"

void *download_thread_func(void *arg) {
  // int rank = *(int *)arg;
  ClientArgs *args = (ClientArgs *)arg;

  get_files_info(args);
  // for (int i = 0; i < (int) args->tracker_data.size(); i++) {
  //   printFileInfo(*args->tracker_data[i]);
  // }

  // printf("Client %d: downloaded files info\n", args->rank);

  for (int i = 0; i < (int)args->client_data->wished_files.size(); i++) {
    // printf("C %d> Downloading file %d|%s with no_chunks %lu\n", args->rank,
    // i,
    //        args->client_data->wished_files[i].c_str(),
    //        args->tracker_data[i]->chunks.size());
    for (int j = 0; j < (int)args->tracker_data[i]->chunks.size(); j++) {
      // printf("C %d> Downloading chunk %d/%lu\n", args->rank, j,
      //        args->tracker_data[i]->chunks.size());
      // printf("C %d j %d\n", args->rank, j);
      if (j > 0 && (j + 1) % 10 == 0) {
        // Update tracker
        update_tracker(args, i, j);
      }
      vector<Seeder> seeder_pool;
      for (int k = 0; k < (int)args->tracker_data[i]->seeders.size(); k++) {
        if (args->tracker_data[i]->seeders[k].id == args->rank) {
          continue;
        }
        if (args->tracker_data[i]->seeders[k].chunks_owned > j) {
          // printf("C %d> Already have chunk %d\n", args->rank, j);
          seeder_pool.push_back(args->tracker_data[i]->seeders[k]);
        }
      }
      int seeder_rank = seeder_pool[rand() % seeder_pool.size()].id;
      // printf("C %d> file %s seeder %d pool size %lu\n", args->rank,
      //        args->tracker_data[i]->filename, seeder_rank,
      //        seeder_pool.size());

      // printf("C %d> Downloading chunk %d/%lu from %d\n", args->rank, j,
      //        args->tracker_data[i]->chunks.size(), seeder_rank);
      download_chunk(args, seeder_rank, i, j);
       // printf("C%d|S%d<%s> Saved chunk %d> %s\n", args->rank,seeder_rank,
      //        args->client_data->wished_files[i].c_str(), j,
      //        args->client_data->owned_files[args->client_data
      //                                                ->wished_files[i]]
      //                                            [j].c_str());

      // printf("Added chunk %d|%d %s\n",seeder_rank, last_chunks.back().idx,
      // last_chunks.back().filename);
    }
    //
    // printf("Ajungi aici? %d\n\n\n\n", args->rank);
    //
    //   // Update tracker
    signal_file_download_finished(args, i);
    save_file(args->client_data->wished_files[i], args);
  }
  signal_all_downloads_finished();

  // printf("Client %d: finished downloading\n", args->rank);
  //

  return NULL;
}

void *upload_thread_func(void *arg) {
  // int rank = *(int *)arg;
  ClientArgs *args = (ClientArgs *)arg;
  // TODO: UPLOAD

  while (args->active) {
    // printf("Peer %d: waiting for request\n", args->rank);
    int flag;
    MPI_Status status;
    status.MPI_ERROR = MPI_SUCCESS;
    MPI_Recv(&flag, 1, MPI_INT, MPI_ANY_SOURCE, PEER_REQUEST, MPI_COMM_WORLD,
             &status);

    switch (status.MPI_SOURCE) {
    case TRACKER_RANK: {
      handle_tracker_message(args, flag, &status);
      break;
    }
    default: {
      // printf("Peer %d: received request from %d\n", args->rank,
      //        status.MPI_SOURCE);
      seed_chunk(args, &status);
      break;
    }
    }
  }

  return NULL;
}

void tracker(int numtasks, int rank) {
  Tracker *tracker_data = new Tracker();
  tracker_data->active = true;
  if (bootstrap_tracker(tracker_data, rank, numtasks) != 0) {
    fprintf(stderr, "Tracker: error in bootstrap\n");
  }

  // fprintf(stdout, "Tracker: boostrap done %lu\n",
  // tracker_data->files.size());

  // printTracker(tracker_data);
  // printFiles(tracker_data);
  //
  MPI_Status *status = new MPI_Status();
  status->MPI_ERROR = MPI_SUCCESS;

  while (tracker_data->active) {
    int data;

    MPI_Recv(&data, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD,
             status);
    // fprintf(stdout, "Tracker: received request %d from %d\n",
    // status->MPI_TAG,
    //         status->MPI_SOURCE);

    switch (status->MPI_TAG) {
    case REQUEST: {
      if (send_files_info(tracker_data, status->MPI_SOURCE, data) != 0) {
        fprintf(stderr, "Tracker: error in sending file info\n");
        // exit(-1);
      }
      break;
    }
    case CLIENT_UPDATE: {
      handle_client_status_update(tracker_data, status);
      break;
    }
    case FINISHED_FILE_DOWNLOAD: {
      handle_finished_file_download(tracker_data, status);
      break;
    }
    case FINISHED_ALL_DOWNLOADS: {
      handle_all_client_downloads_finished(tracker_data, status);
      break;
    }
    default: {
      fprintf(stderr, "Tracker: received unknown tag %d\n", status->MPI_TAG);
      break;
    }
    }
  }

  if (signal_shutdown_to_clients(tracker_data, numtasks) != 0) {
    fprintf(stderr, "Tracker: error in sending shutdown signal\n");
    // exit(-1);
  }

}

void peer(int numtasks, int rank) {
  ClientArgs *args = new ClientArgs();
  args->active = true;
  args->rank = rank;
  args->client_data = read_file(rank);
  if (args->client_data == NULL) {
    fprintf(stderr, "Client %d: error in reading file\n", rank);
    exit(-1);
  }
  // printClientData(args->client_data);
  
  // print_client_data(args->client_data, logfile);
  send_seed_info_to_tracker(args->client_data, rank);
  if (wait_for_tracker(rank) != 0) {
    fprintf(stderr, "Client %d: error in waiting for tracker\n", rank);
    exit(-1);
  }

  pthread_t download_thread;
  pthread_t upload_thread;
  void *status;
  int r;

  r = pthread_create(&download_thread, NULL, download_thread_func,
                     (void *)args);
  if (r) {
    printf("Eroare la crearea thread-ului de download\n");
    exit(-1);
  }

  r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *)args);
  if (r) {
    printf("Eroare la crearea thread-ului de upload\n");
    exit(-1);
  }

  r = pthread_join(download_thread, &status);
  if (r) {
    printf("Eroare la asteptarea thread-ului de download\n");
    exit(-1);
  }

  r = pthread_join(upload_thread, &status);
  if (r) {
    printf("Eroare la asteptarea thread-ului de upload\n");
    exit(-1);
  }

  // pthread_barrier_destroy(&barrier);
}

int main(int argc, char *argv[]) {
  int numtasks, rank;

  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  if (provided < MPI_THREAD_MULTIPLE) {
    fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
    exit(-1);
  }
  MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  if (rank == TRACKER_RANK) {
    tracker(numtasks, rank);
  } else {
    peer(numtasks, rank);
  }
  MPI_Finalize();
}
