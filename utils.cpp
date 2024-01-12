#include <iostream>

#include "tema3.h"
#include <string>


int pack_file(FileInfo *file, char *buffer, int *size) {
  int offset = 0;

  pack_data_int(buffer + offset, file->leechers);
  offset += file->leechers.size() * sizeof(int);


  for (int i = 0; i < (int)file->seeders.size(); i++) {
    memcpy(buffer + offset, &file->seeders[i], sizeof(Seeder));
    offset += sizeof(Seeder);
  }

  pack_data(buffer + offset, file->chunks, HASH_SIZE);
  // printf("Cum coaie\n");
  offset += (HASH_SIZE) * file->chunks.size();

  // printf("Aici????\n");

  if (*size != offset)
    printf("Size mismatch: %d %d\n", *size, offset);


  return 0;
}

int unpack_file(FileInfo *file, File_Metadata *data, char *buffer, int *size) {
  int offset = 0;
  unpack_data_int(buffer + offset, file->leechers, data->no_leechers);
  offset += data->no_leechers * sizeof(int);

  for (int i = 0; i < data->no_seeders; i++) {
    Seeder seeder;
    memcpy(&seeder, buffer + offset, sizeof(Seeder));
    // printf("Unpacked SEED %d %d\n", seeder.id, seeder.chunks_owned);
    file->seeders.push_back(seeder);
    offset += sizeof(Seeder);
  }

  unpack_data(buffer + offset, file->chunks, data->no_chunks, HASH_SIZE);
  offset += (HASH_SIZE) * data->no_chunks;
  // you are smart, figure it out
  //
  if (*size != offset)
    printf(">>>>>Size mismatch: %d %d\n", *size, offset);




  return 0;
}

void pack_data(char *buffer, vector<string> chunks, int data_size) {
  for (int i = 0; i < (int)chunks.size(); ++i) {
    // printf("Pula mea %d \n",chunks[i].size() );
    memcpy(buffer + i * (data_size), chunks[i].c_str(), data_size);
  }
}
void pack_data_int(char *buffer, vector<int> chunks) {
  memcpy(buffer, chunks.data(), chunks.size() * sizeof(int));
  // fprintf(stdout, "Pack %lu\n", chunks.size());
}

void unpack_data(char *buffer, vector<string> &chunks, int chunk_count,
                 int data_size) {
  for (int i = 0; i < chunk_count; ++i) {
    string chunk =
        string(buffer + i * (data_size), data_size);
    // printf("Unpacked chunk %s\n", chunk.c_str());
    chunks.push_back(chunk);
  }
}

void unpack_variable_data(char *buffer, vector<string> &chunks, int chunk_count,
                 int data_size) {
  for (int i = 0; i < chunk_count; ++i) {
    string chunk =
        string(buffer + i * (data_size), strlen(buffer + i * (data_size)));
    chunks.push_back(chunk);
  }
}

void unpack_data_int(char *buffer, vector<int> &chunks, int chunk_count) {
  chunks.resize(chunk_count);
  memcpy(chunks.data(), buffer, chunk_count * sizeof(int));

  // fprintf(stdout, "Unpack %d\n", chunk_count);
}

void printFileInfo(const FileInfo& fileInfo) {
    std::cout << "File Info:\n";
    std::cout << "Filename: " << fileInfo.filename << "\n";

    std::cout << "Leechers:\n";
    for (const int& leecher : fileInfo.leechers) {
        std::cout << leecher << " ";
    }
    std::cout << "\n";

    std::cout << "Seeders:\n";
    for (const Seeder& seeder : fileInfo.seeders) {
        std::cout << "ID: " << seeder.id << ", Chunks Owned: " << seeder.chunks_owned << "\n";
    }

    std::cout << "Chunks:\n";
    for (const std::string& chunk : fileInfo.chunks) {
        std::cout << chunk << "\n";
    }
}

void printClientData(ClientData *client_data) {
  // Print owned files
  printf("Owned Files:\n");
  for (const auto& entry : client_data->owned_files) {
    printf("  %s:\n", entry.first.c_str());
    for (const auto& chunk : entry.second) {
      printf("    %s\n", chunk.c_str());
    }
  }

  // Print wished files
  printf("Wished Files:\n");
  for (const auto& filename : client_data->wished_files) {
    printf("  %s\n", filename.c_str());
  }
}


void printFiles(const Tracker* tracker) {
    if (!tracker) {
        printf("Invalid Tracker pointer\n");
        return;
    }

    printf("Files:\n");

    for (const auto& fileEntry : tracker->files) {
        const std::string& filename = fileEntry.first;
        const FileInfo* fileInfo = fileEntry.second;

        printf("  Filename: %s\n", fileInfo->filename);

        // Print leechers
        printf("    Leechers:");
        for (int leecher : fileInfo->leechers) {
            printf(" %d", leecher);
        }
        printf("\n");

        // Print seeders
        printf("    Seeders:\n");
        for (const Seeder& seeder : fileInfo->seeders) {
            // Assuming you have some way to print Seeder information
            // You may need to adjust this part according to Seeder structure
            printf("      Seeder Information\n");
        }

        // Print chunks
        printf("    Chunks:\n");
        for (const std::string& chunk : fileInfo->chunks) {
            printf("%s\n", chunk.c_str());
        }
        printf("\n");
    }
}
