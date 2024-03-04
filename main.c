#include <stdio.h>
#include <stdlib.h>

typedef struct root_entry
{
    unsigned char file_name[11];
    unsigned char attributes;
    unsigned char reserved_windows_nt;
    unsigned char creation_time;
    unsigned short time_creation;
    unsigned short date_creation;
    unsigned short last_accessed;
    unsigned short high_first_cluster;
    unsigned short last_modification_time;
    unsigned short last_modification_date;
    unsigned short low_first_cluster;
    unsigned int file_size;
} __attribute__((packed)) root_entry_t;

typedef struct fat_BS
{
    unsigned char bootjmp[3];
    unsigned char oem_name[8];
    unsigned short bytes_per_sector;
    unsigned char sectors_per_cluster;
    unsigned short reserved_sector_count;
    unsigned char table_count;
    unsigned short root_entry_count;
    unsigned short total_sectors_16;
    unsigned char media_type;
    unsigned short table_size_16;
    unsigned short sectors_per_track;
    unsigned short head_side_count;
    unsigned int hidden_sector_count;
    unsigned int total_sectors_32;

    // this will be cast to it's specific type once the driver actually knows what type of FAT this is.
    unsigned char extended_section[54];

} __attribute__((packed)) fat_BS_t;

root_entry_t *get_valid_entries(fat_BS_t boot_record, FILE *fp, int *valid_entries_size)
{
    root_entry_t root_entry;
    root_entry_t *valid_entries = malloc(0 * sizeof(root_entry_t));
    *valid_entries_size = 0;

    for (int i = 0; i < boot_record.root_entry_count; i++)
    {
        fread(&root_entry, sizeof(root_entry_t), 1, fp);
        if (root_entry.attributes == 0x0F)
        {
            printf("[ Skipped long file name ]\n");
            continue;
        }
        else if (root_entry.file_name[0] == 0xE5)
        {
            printf("[ Skipped deleted entry ]\n");
            continue;
        }
        else if (root_entry.file_name[0] == 0)
        {
            printf("[ Finished reading root entries ]\n");
            break;
        }
        else
        {
            valid_entries = realloc(valid_entries, ++*valid_entries_size * sizeof(root_entry_t));
            valid_entries[*valid_entries_size - 1] = root_entry;
        }
    }
    return valid_entries;
}

void print_root_entry_info(root_entry_t entry)
{
    printf("File Name: %s \n", entry.file_name);
    printf("Attributes: %d \n", entry.attributes);
    printf("Reserved Windows NT: %d \n", entry.reserved_windows_nt);
    printf("Creation Time: %d \n", entry.creation_time);
    printf("Time Creation: %d \n", entry.time_creation);
    printf("Date Creation: %d \n", entry.date_creation);
    printf("Last Accessed: %d \n", entry.last_accessed);
    printf("High First Cluster: %x \n", entry.high_first_cluster);
    printf("Last Modification Time: %d \n", entry.last_modification_time);
    printf("Last Modification Date: %d \n", entry.last_modification_date);
    printf("Low First Cluster: %x \n", entry.low_first_cluster);
    printf("File Size: %d \n\n", entry.file_size);
}

unsigned short *get_clusters(root_entry_t entry, fat_BS_t boot_record, FILE *fp, int *clusters_size)
{
    int fat_table_location = boot_record.bytes_per_sector * boot_record.reserved_sector_count;
    *clusters_size = 1;
    unsigned short *clusters = malloc(*clusters_size * sizeof(unsigned short));
    unsigned short current_cluster = entry.low_first_cluster;
    clusters[0] = current_cluster;
    printf("Iniciando Cluster Chain: %x\n", current_cluster);
    while (1)
    {
        int cluster_location = fat_table_location + current_cluster * 2;
        printf("Cluster atual: %x Localização: %d\n", current_cluster, cluster_location);
        fseek(fp, fat_table_location + current_cluster * 2, SEEK_SET);
        fread(&current_cluster, sizeof(unsigned short), 1, fp);
        if (current_cluster == 0xFFFF || current_cluster == 0xFFF8)
            break;
        else
        {
            clusters = realloc(clusters, ++(*clusters_size) * sizeof(unsigned short));
            clusters[*clusters_size - 1] = current_cluster;
        }
    }
    printf("Clusters finais (%d): [", *clusters_size);
    for (int i = 0; i < *clusters_size; i++)
        printf("%x ", clusters[i]);
    printf("]\n");
    return clusters;
}

char *get_data(root_entry_t entry, fat_BS_t boot_record, FILE *fp, unsigned short *clusters, int clusters_size)
{
    int root_location = boot_record.bytes_per_sector * boot_record.reserved_sector_count + boot_record.bytes_per_sector * boot_record.table_size_16 * boot_record.table_count;
    int data_location = root_location + boot_record.root_entry_count * sizeof(root_entry_t);
    char *data = malloc(entry.file_size + 1);
    data[entry.file_size] = '\0';
    int cluster_size = boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
    int temp_file_size = entry.file_size;
    printf("Tamanho cluster: %d\n", cluster_size);
    for (int i = 0; i < clusters_size; i++)
    {
        int cluster_shift = ((clusters[i] - 2) * boot_record.sectors_per_cluster * boot_record.bytes_per_sector);
        int cluster_location = data_location + cluster_shift;
        printf("Cluster %d\n", cluster_location);
        fseek(fp, cluster_location, SEEK_SET);
        if (i == clusters_size - 1)
        {
            printf("Tamanho restante: %d\n", temp_file_size);
            fread(&data[i * cluster_size], temp_file_size, 1, fp);
        }
        else
        {
            fread(&data[i * cluster_size], cluster_size, 1, fp);
            printf("Tamanho atual: %d\n", temp_file_size);
            temp_file_size -= cluster_size;
        }
    }
    return data;
}

int main(int argc, char const *argv[])
{

    FILE *fp;
    fat_BS_t boot_record;

    fp = fopen(argv[1], "rb");
    fseek(fp, 0, SEEK_SET);
    fread(&boot_record, sizeof(fat_BS_t), 1, fp);

    // Boot Record Info

    printf("Boot record jump code: %c%c%c \n", boot_record.bootjmp[0], boot_record.bootjmp[1], boot_record.bootjmp[2]);
    printf("OEM name: %s \n", boot_record.oem_name);
    printf("Bytes per sector %hd \n", boot_record.bytes_per_sector);
    printf("Sector per cluster %x \n", boot_record.sectors_per_cluster);
    printf("Reserved sector count: %hu \n", boot_record.reserved_sector_count);
    printf("Table count: %hu \n", boot_record.table_count);
    printf("Root entry count: %hu \n", boot_record.root_entry_count);
    printf("Total sectors 16: %hu \n", boot_record.total_sectors_16);
    printf("Media type: %x \n", boot_record.media_type);
    printf("Table size 16: %hu \n", boot_record.table_size_16);
    printf("Sectors per track: %hu \n", boot_record.sectors_per_track);
    printf("Head side count: %hu \n", boot_record.head_side_count);
    printf("Hidden sector count: %u \n", boot_record.hidden_sector_count);
    printf("Total sectors 32: %u \n", boot_record.total_sectors_32);

    // Read Root Directory

    printf("\n== ROOT ENTRIES ==\n\n");

    int root_location = boot_record.bytes_per_sector * boot_record.reserved_sector_count + boot_record.bytes_per_sector * boot_record.table_size_16 * boot_record.table_count;
    printf("LOCATION %d\n", root_location);
    printf("Size of entry: %d\n\n", sizeof(root_entry_t));
    fseek(fp, root_location, SEEK_SET);

    int valid_entries_size;
    root_entry_t *valid_entries = get_valid_entries(boot_record, fp, &valid_entries_size);

    for (int i = 0; i < valid_entries_size; i++)
    {
        printf("[ Root Entry %d] \n\n", i);
        print_root_entry_info(valid_entries[i]);
    }

    int option = -1;
    int data_location = root_location + boot_record.root_entry_count * sizeof(root_entry_t);
    while (option != 0)
    {
        for (int i = 0; i < valid_entries_size; i++)
        {
            printf("%d. %s\n", i + 1, valid_entries[i].file_name);
        }
        printf("Escreva o indice da entrada, ou 0 para sair:\n");
        scanf("%d", &option);
        printf("\n\n");
        if (option > 0 && option <= valid_entries_size)
        {
            print_root_entry_info(valid_entries[option - 1]);
            int fat_table_location = boot_record.bytes_per_sector * boot_record.reserved_sector_count;
            int clusters_size = 1;
            unsigned short *clusters = get_clusters(valid_entries[option - 1], boot_record, fp, &clusters_size);
            printf("\nMontando os dados...\n\n");
            if (valid_entries[option - 1].file_size != 0)
            {
                char *data = get_data(valid_entries[option - 1], boot_record, fp, clusters, clusters_size);
                printf("\n== Conteúdo == (Um \\n extra)\n\n");
                printf("%s\n", data);
                free(data);
                free(clusters);
            }
            else
                printf("Vazio ou Sub-Diretório\n");
        }
        else if (option < 0 || option > valid_entries_size)
        {
            printf("Opção inválida\n");
        }
    }

    free(valid_entries);

    return 0;
}
