#define GELBOORU_DOWNLOADER_IMPLEMENTATION
#include "gelbooru_downloader.h"

void search_tags(const char *query) {
    printf("Search tags: %s\n", query);
    gelbooru *gbooru = gelbooru_create();
    if (gbooru == NULL) {
        printf("Failed to create Gelbooru object\n");
        return;
    }

    vector* tags = gelbooru_tag_search(gbooru, query);
    if (tags == NULL) {
        printf("Failed to search tags\n");
        gelbooru_destroy(gbooru);
        return;
    }

    printf("Results: ");
    if (vector_size(tags) <= 0) {
        printf("not found\n");
    } else {
        printf("\n");
        printf("%-50s | %-10s\n", "Tag", "Post count");

        for (int i = 0; i < vector_size(tags); i++) {
            gelbooru_tag *tag = vector_index(tags, i);
            printf("%-50s | %-10d\n", tag->tag, tag->post_count);
        }
        gelbooru_tag_list_free(tags);
    }

    gelbooru_destroy(gbooru);
}



void download_images(int tags_count, char **input_tags) {
    vector *tags = vector_create();
    if (tags == NULL) {
        printf("Failed to create tags vector\n");
        return;
    }

    gelbooru* gbooru = gelbooru_create();
    if (gbooru == NULL) {
        printf("Failed to create Gelbooru object\n");
        return;
    }

    for (int i = 0; i < tags_count; i++) {
        char *tag = strdup(input_tags[i]);
        if (tag == NULL) {
            printf("Failed to allocate mem for tag %s\n", input_tags[i]);
            break;
        }
        if (vector_push_back(tags, tag) != 0) {
            printf("Failed to push tag %s to vector\n", tag);
            free(tag);
        }
    }

    printf("Download images with tags: ");
    for (int i = 0; i < vector_size(tags); i++) {
        printf("%s, ", vector_index(tags, i));
    }
    printf("\n");


    // formats
    gelbooru_add_image_format(gbooru, "jpg");
    gelbooru_add_image_format(gbooru, "jpeg");
    gelbooru_add_image_format(gbooru, "png");
    gelbooru_add_image_format(gbooru, "gif");
    vector *formats = gelbooru_get_image_formats(gbooru);
    printf("Image formats: ");
    for (int i = 0; i < vector_size(formats); i++) {
        printf("%s, ", vector_index(formats, i));
    }
    printf("\n");



    // params
    gelbooru_set_parser_sleep_ms(gbooru, 500);
    gelbooru_set_downloader_sleep_ms(gbooru, 100);
    printf("Parser sleep %d ms\n", gbooru->parser_sleep_ms);
    printf("Downloader sleep %d ms\n", gbooru->downloader_sleep_ms);

    int thread_count = 10;
    gelbooru_set_download_thread_count(gbooru, thread_count);
    printf("Download threads: %d\n", gbooru->download_thread_count);


    // download
    gelbooru_download(gbooru, tags);


    // cleanup
    for (int i = 0; i < vector_size(tags); i++) {
        char* t = vector_index(tags, i);
        free(t);
    }
    vector_destroy(tags);
    gelbooru_destroy(gbooru);
}


int main(int argc, char **argv) {
    printf("\033[36mGelbooru Downloader by AliceZed\033[0m\n");

    char msg[] = "Usage:\n"
                "gbooru search-tags <query>\n"
                "gbooru download <tag1> [<tag2> ...]\n";

    if (argc < 3) {
        printf(msg);
        return 1;
    }

    if (strcmp(argv[1], "search-tags") == 0) {
        search_tags(argv[2]);
    }
    else if (strcmp(argv[1], "download") == 0) {
        download_images(argc - 2, argv + 2);
    }
    else {
        printf(msg);
        return 1;
    }
    return 0;
}