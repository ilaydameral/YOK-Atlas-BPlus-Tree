#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#define MAX_STR 128
#define MAX_RUN_FILES 100

int order = 4;
int insertionMode = 0; // seq ya da bulk

typedef struct University {
    char uuid[MAX_STR];
    char name[MAX_STR];
    double score;
    struct University* next; // linked list için 
} University;

typedef struct BPlusNode {
    int isLeaf;
    int numberKeys;
    char** keys; // bölüm isimleri
    struct BPlusNode** children;
    University** universityLists; // üni listesi her department için 
    struct BPlusNode* parent;
    struct BPlusNode* next;
    struct BPlusNode* prev;
} BPlusNode;

typedef struct {
    char uuid[MAX_STR];
    char universityName[MAX_STR];
    char department[MAX_STR];
    double score;
    int frozen; // sıralamada kullanılmayacak kayıtlar için
} Record;

typedef struct {
    int splitCount;
    int treeHeight;
    size_t memoryBytes;
    double buildTime;

} Metrics;

Metrics metrics = {0}; // başta split yok , root boş
BPlusNode* root = NULL;

// recordları karşılararak sıralama
int compareRecords(const void* a, const void* b) {
    Record* r1 = (Record*)a;
    Record* r2 = (Record*)b;
    int d = strcmp(r1->department, r2->department);
    return (d != 0) ? d : (r1->score < r2->score ? 1 : -1); //yüksek skor önce
}


// heap i aşağıya doğru yerleştirir
void heapifyDown(Record* heap, int size, int i) {
    int smallest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;

    if (left < size && compareRecords(&heap[left], &heap[smallest]) < 0)
        smallest = left;

    if (right < size && compareRecords(&heap[right], &heap[smallest]) < 0)
        smallest = right;

    if (smallest != i) {
        Record temp = heap[i];
        heap[i] = heap[smallest];
        heap[smallest] = temp;
        heapifyDown(heap, size, smallest);
    }
}

// heap e yukarıdan veri ekleme
void heapifyUp(Record* heap, int i) {
    while (i > 0 && compareRecords(&heap[i], &heap[(i - 1) / 2]) < 0) {
        Record temp = heap[i];
        heap[i] = heap[(i - 1) / 2];
        heap[(i - 1) / 2] = temp;
        i = (i - 1) / 2;
    }
}

// heap e yeni veri ekleme
void heapPush(Record* heap, int* size, Record r) {
    heap[*size] = r;
    heapifyUp(heap, *size); // uygun yere yerleştir
    (*size)++;
}

// heap ten en üstteki veriyi çıkarma
Record heapPop(Record* heap, int* size) {
    Record top = heap[0];
    heap[0] = heap[--(*size)];
    heapifyDown(heap, *size, 0); // uygun yere

    return top;
}

// verilerde string temizleme
void cleanString(char* s) {
    char* p = s;

    // Baştaki " varsa kaldır
    if (*p == '"') p++;

    // Sondaki " varsa kaldır
    char* end = p + strlen(p) - 1;
    if (*end == '"') *end = '\0';

    // Kopyala temiz şekilde
    memmove(s, p, strlen(p) + 1);
}

void parseCSVLine(char* line, Record* r) {
    char *lastComma, *secondComma, *firstComma;

    // burada yaptıgımız teker teker ilerideki virgülü ayraç yapmak yerine 
    //son virgülü ayraç yapıp departmant a kadar sıra sıra gitmek 
    // bu sayede eğer bölümün adında virgül varsa onu bir ayraç olarak kullanmayacağız
    lastComma = strrchr(line, ',');
    if (!lastComma) return;
    r->score = atof(lastComma + 1);
    *lastComma = '\0';
    firstComma = strchr(line, ',');
    if (!firstComma) return;
    *firstComma = '\0';

    secondComma = strchr(firstComma + 1, ',');
    if (!secondComma) return;
    *secondComma = '\0';
    strncpy(r->uuid, line, MAX_STR);
    strncpy(r->universityName, firstComma + 1, MAX_STR);
    strncpy(r->department, secondComma + 1, MAX_STR);

    // Temizlik: baştaki ve sondaki tırnakları ve boşlukları sil
    cleanString(r->uuid);
    cleanString(r->universityName);
    cleanString(r->department);

    r->frozen = 0;
}

// yeni üni oluşturma 
University* createUniversity(const char* uuid, const char* name, double score) {
    University* uni = malloc(sizeof(University)); // dinamik bellek ayrılır
    strcpy(uni->uuid, uuid);
    strcpy(uni->name, name);
    uni->score = score;
    uni->next = NULL; // linked list başı

    return uni;
}

// heap ile sıralanmışüni listesi oluşturma
University* buildUniList(Record* records, int count) {
    for (int i = count / 2 - 1; i >= 0; i--) // max-heap oluşturma
        heapifyDown(records, count, i);

    University* head = NULL;
    University* tail = NULL;

    while (count > 0) {
        Record r = heapPop(records, &count);
        University* uni = createUniversity(r.uuid, r.universityName, r.score);

        if (!head) // hiç üni yoksa
            head = tail = uni;
        else { // üni varsa sona ekleme
            tail->next = uni;
            tail = uni;
        }
    }

    return head;
}

// yeni b+ tree node u oluşturma
BPlusNode* createNode(int isLeaf) {
    BPlusNode* node = malloc(sizeof(BPlusNode)); // bellek yani rami ayır
    node->isLeaf = isLeaf;
    node->numberKeys = 0;
    node->keys = malloc((order - 1) * sizeof(char*));
    node->children = malloc(order * sizeof(BPlusNode*));
    node->universityLists = malloc((order - 1) * sizeof(University*));
    
    for (int i = 0; i < order - 1; i++) { // başta hepsi null
        node->keys[i] = NULL;
        node->universityLists[i] = NULL;
    }

    for (int i = 0; i < order; i++) 
        node->children[i] = NULL;
    node->parent = node->next = node->prev = NULL; // başta bağlı değil

    return node;
}

void insertIntoParent(BPlusNode* left, const char* key, BPlusNode* right);

// leaf node u ikiye bölme
void splitLeafNode(BPlusNode* leaf) {
    metrics.splitCount++;
    BPlusNode* newLeaf = createNode(1);
    int split = (order - 1) / 2;

    // esli leaf node daki verilerin yarısı yeni leaf node a 
    for (int i = split, j = 0; i < order - 1; i++, j++) {
        newLeaf->keys[j] = leaf->keys[i];
        newLeaf->universityLists[j] = leaf->universityLists[i];
        leaf->keys[i] = NULL; // eski node dan sil
        leaf->universityLists[i] = NULL;
    }

    newLeaf->numberKeys = (order - 1) - split;
    leaf->numberKeys = split;

    // iki leaf node birbirine bağlanıyor
    newLeaf->next = leaf->next;
    if (newLeaf->next) 
        newLeaf->next->prev = newLeaf;
    leaf->next = newLeaf;
    newLeaf->prev = leaf;

    newLeaf->parent = leaf->parent;
    insertIntoParent(leaf, newLeaf->keys[0], newLeaf);
}

// internal node bölme
void splitInternalNode(BPlusNode* node) {
    metrics.splitCount++;
    BPlusNode* newNode = createNode(0);
    int mid = (order - 1) / 2;
    char* middleKey = node->keys[mid];

    // sağ child lar yeni node a
    for (int i = mid + 1, j = 0; i < order - 1; i++, j++) {
        newNode->keys[j] = node->keys[i];
        newNode->children[j] = node->children[i];

        if (newNode->children[j]) 
            newNode->children[j]->parent = newNode;
        node->keys[i] = NULL;
        node->children[i] = NULL;
    }

    // yeni child için bağlantıları güncelleniyor
    newNode->children[(order - 1) - mid - 1] = node->children[order - 1];
    if (newNode->children[(order - 1) - mid - 1]) 
        newNode->children[(order - 1) - mid - 1]->parent = newNode;
    node->children[order - 1] = NULL;

    newNode->numberKeys = (order - 1) - mid - 1;
    node->numberKeys = mid;
    newNode->parent = node->parent;

    insertIntoParent(node, middleKey, newNode); // yeni node da parent a ekleniyor
}

// parent node a ekleme
void insertIntoParent(BPlusNode* left, const char* key, BPlusNode* right) {
    if (!left->parent) {
        BPlusNode* newRoot = createNode(0); // yeni internal node
        newRoot->keys[0] = strdup(key);
        newRoot->children[0] = left;
        newRoot->children[1] = right;
        newRoot->numberKeys = 1;
        left->parent = newRoot;
        right->parent = newRoot;
        root = newRoot;

        return;
    }

    BPlusNode* parent = left->parent; //internal node a key ekleniyor
    int index = 0;

    while (index <= parent->numberKeys && parent->children[index] != left) 
        index++;

    for (int i = parent->numberKeys; i > index; i--) {
        parent->keys[i] = parent->keys[i - 1];
        parent->children[i + 1] = parent->children[i];
    }

    parent->keys[index] = strdup(key);
    parent->children[index + 1] = right;
    right->parent = parent;
    parent->numberKeys++;

    if (parent->numberKeys == order - 1) // parent node dolduysa split yapılıyor
        splitInternalNode(parent);
}

// sequential insertion yöntemiyle b+ tree ye record ekleme
void insertSequential(Record r) { 
    if (!root) 
        root = createNode(1);
    BPlusNode* leaf = root;

    // leaf node a ulaşmak için binary search 
    while (!leaf->isLeaf) {
        int i = 0;
        while (i < leaf->numberKeys && strcmp(r.department, leaf->keys[i]) > 0) 
            i++;
        leaf = leaf->children[i];
    }

    int i = 0;
    while (i < leaf->numberKeys && strcmp(r.department, leaf->keys[i]) > 0) 
        i++;

    // mevcut departman varsa sıralı ekleme
    if (i < leaf->numberKeys && strcmp(leaf->keys[i], r.department) == 0) {
        University* newUni = createUniversity(r.uuid, r.universityName, r.score);
        University** head = &leaf->universityLists[i];

        if (*head == NULL || newUni->score > (*head)->score) {
            newUni->next = *head;
            *head = newUni;
        } 
        else {
            University* current = *head;
            while (current->next && current->next->score >= newUni->score)
                current = current->next;
            newUni->next = current->next;
            current->next = newUni;
        }

        return;
    }

    // departman ilk kez giriliyorsa
    for (int j = leaf->numberKeys; j > i; j--) {
        leaf->keys[j] = leaf->keys[j - 1];
        leaf->universityLists[j] = leaf->universityLists[j - 1];
    }

    leaf->keys[i] = strdup(r.department); // yeni bölüm. ve üni ekleniyor
    leaf->universityLists[i] = NULL;

    University* newUni = createUniversity(r.uuid, r.universityName, r.score);
    leaf->universityLists[i] = newUni;
    newUni->next = NULL;

    leaf->numberKeys++;

    if (leaf->numberKeys == order - 1) // leaf node dolarsa split
        splitLeafNode(leaf);
}

// csv den belirli büyüklükte run lar oluşturma
void generateSortedRuns(const char* inputFile, int bufferSize, int* runCount) {
    FILE* f = fopen(inputFile, "r");
    if (!f) {
        perror("CSV file is not opening!");
        exit(1);
    }

    char line[512];
    fgets(line, sizeof(line), f); // header

    Record* heap = malloc(bufferSize * sizeof(Record));
    int heapSize = 0;
    *runCount = 0;

    // ilk buffer dolduruluyor
    while (heapSize < bufferSize && fgets(line, sizeof(line), f)) {
        parseCSVLine(line, &heap[heapSize]);
        heap[heapSize].frozen = 0;
        heapSize++;
    }

    // ilk heapify
    for (int i = heapSize / 2 - 1; i >= 0; i--)
        heapifyDown(heap, heapSize, i);

    Record* tempHeap = malloc(bufferSize * sizeof(Record));
    int tempSize = 0;

    char lastDepartment[MAX_STR] = "";
    double lastScore = -1.0;

    char fname[32];
    FILE* out = NULL;

    while (heapSize > 0) {
        sprintf(fname, "run_%d.csv", (*runCount)++);
        out = fopen(fname, "w");

        int activeSize = heapSize;

        // bir run oluşturulacak
        while (activeSize > 0) {
            Record top = heap[0];
            fprintf(out, "%s,%s,%s,%.2f\n", top.uuid, top.universityName,
                    top.department, top.score);
            strcpy(lastDepartment, top.department);
            lastScore = top.score;

            // yeni kayıt okuma
            if (fgets(line, sizeof(line), f)) {
                Record newRecord;
parseCSVLine(line, &newRecord);
                newRecord.frozen = (strcmp(newRecord.department, lastDepartment) < 0) || (strcmp(newRecord.department, lastDepartment) == 0 && newRecord.score < lastScore);

                if (!newRecord.frozen)
                    heap[0] = newRecord;
                else {
                    tempHeap[tempSize++] = newRecord; // frozen eleman temp heap e  
                    heap[0] = heap[--activeSize]; // son eleman root a
                }
            } 
            else //dosya bitti
                heap[0] = heap[--activeSize];

            heapifyDown(heap, activeSize, 0);
        }

        fclose(out);

        for (int i = 0; i < tempSize; i++) // frozen olanlar yeni heap e
            heap[i] = tempHeap[i];
        heapSize = tempSize;
        tempSize = 0;

        for (int i = heapSize / 2 - 1; i >= 0; i--)
            heapifyDown(heap, heapSize, i);
    }

    free(heap);
    free(tempHeap);
    fclose(f);
}

// sıralı run ları birleştirme
void mergeSortedRuns(const char* outputFile, int runCount) {
    FILE* files[MAX_RUN_FILES];
    Record buffer[MAX_RUN_FILES];
    int active[MAX_RUN_FILES];
    char line[512];

    // Her run dosyasını aç ve ilk satırı oku
    for (int i = 0; i < runCount; i++) {
        char fname[32];
        sprintf(fname, "run_%d.csv", i);
        files[i] = fopen(fname, "r");
        active[i] = fgets(line, sizeof(line), files[i]) != NULL;

        if (active[i])
            parseCSVLine(line, &buffer[i]);
    }

    FILE* out = fopen(outputFile, "w");
    fprintf(out, "UUID,University,Department,Score\n");

    // Sıralı şekilde birleştirme işlemi
    while (1) {
        int min = -1;

        for (int i = 0; i < runCount; i++) {
            if (!active[i]) continue;
            if (min == -1 || compareRecords(&buffer[i], &buffer[min]) < 0)
                min = i;
        }

        if (min == -1) break;

        fprintf(out, "%s,%s,%s,%.2f\n", buffer[min].uuid, buffer[min].universityName,
                buffer[min].department, buffer[min].score);

        if (fgets(line, sizeof(line), files[min]))
            parseCSVLine(line, &buffer[min]);
        else
            active[min] = 0;
    }

    fclose(out);

    // Her dosyayı kapat ve sil
    for (int i = 0; i < runCount; i++) {
        if (files[i]) {
            char fname[32];
            sprintf(fname, "run_%d.csv", i);
            fclose(files[i]);
            if (remove(fname) != 0) {
                perror(fname);
            }
        }
    }
}

// çok anlaşşılmayan bir kısım
void bulkLoadFromSortedCSV(const char* sortedFile) {
    FILE* f = fopen(sortedFile, "r");
    if (!f) {
        perror("Sorted CSV file error!");
        exit(1);
    }

    char line[512];
    fgets(line, sizeof(line), f); 

    Record r;
    char prevDept[MAX_STR] = "";
    Record* records = malloc(100 * sizeof(Record));
    int count = 0, capacity = 100;

    BPlusNode* currentLeaf = createNode(1);
    root = currentLeaf;

    while (fgets(line, sizeof(line), f)) {
        parseCSVLine(line, &r);

        if (strcmp(prevDept, "") != 0 && strcmp(prevDept, r.department) != 0) {
            University* uniList = buildUniList(records, count);
            currentLeaf->keys[currentLeaf->numberKeys] = strdup(prevDept);
            currentLeaf->universityLists[currentLeaf->numberKeys] = uniList;
            currentLeaf->numberKeys++;

            // Eğer bu leaf dolduysa yeni leaf aç
            if (currentLeaf->numberKeys == order - 1) {
                BPlusNode* newLeaf = createNode(1);
                currentLeaf->next = newLeaf;
                newLeaf->prev = currentLeaf;
                currentLeaf = newLeaf;
            }

            count = 0;
        }

        if (count >= capacity) {
            capacity *= 2;
            records = realloc(records, capacity * sizeof(Record));
        }

        records[count++] = r;
        strcpy(prevDept, r.department);
    }

    if (count > 0) {
        University* uniList = buildUniList(records, count);
        currentLeaf->keys[currentLeaf->numberKeys] = strdup(prevDept);
        currentLeaf->universityLists[currentLeaf->numberKeys] = uniList;
        currentLeaf->numberKeys++;
    }

    free(records);
    fclose(f);

    
    if (root->isLeaf && root->next) {
        BPlusNode** leaves = malloc(1000 * sizeof(BPlusNode*));
        int leafCount = 0;

        BPlusNode* temp = root;
        while (temp) {
            leaves[leafCount++] = temp;
            temp = temp->next;
        }

        while (leafCount > 1) {
            int newCount = 0;
            BPlusNode** newLevel = malloc(1000 * sizeof(BPlusNode*));

            for (int i = 0; i < leafCount; i += order) {
                BPlusNode* internal = createNode(0);
                int limit = ((i + order) < leafCount) ? (i + order) : leafCount;

                for (int j = i; j < limit; j++) {
                    internal->children[j - i] = leaves[j];
                    leaves[j]->parent = internal;

                    if (j > i)
                        internal->keys[j - i - 1] = strdup(leaves[j]->keys[0]);
                }

                internal->numberKeys = limit - i - 1;
                newLevel[newCount++] = internal;
            }

            free(leaves);
            leaves = newLevel;
            leafCount = newCount;
            root = leaves[0];
        }
    }
}

void printTreeStructure(BPlusNode* node, int level) {
    if (!node) 
        return;

    printf("%*s", level * 4, "");
    if (node->isLeaf) {
        printf("- Leaf\n");
        for (int i = 0; i < node->numberKeys; i++) {
            printf("%*s- \"%s\"\n", (level + 1) * 4, "", node->keys[i]);
        }
    } else {
        printf("- Internal\n");
        for (int i = 0; i < node->numberKeys; i++) {
            printf("%*s- \"%s\"\n", (level + 1) * 4, "", node->keys[i]);
        }
        for (int i = 0; i <= node->numberKeys; i++) {
            printTreeStructure(node->children[i], level + 1);
        }
    }
}

// b+ tree yi sorted şekilde arar ve yerleşme sırasına göre üni bulur
void searchUniRecord(const char* department, int rank) {
    clock_t start = clock(); // seek time için
    BPlusNode* current = root;

    // tree üzerinden binary search ile leaf node a ulaşıyoruz
    while (current && !current->isLeaf) {
        int i = 0;
        while (i < current->numberKeys && strcmp(department, current->keys[i]) > 0) 
            i++;
        current = current->children[i];
    }

    // leaf node içinde key arama
    while (current) {
        for (int i = 0; i < current->numberKeys; i++) {
            if (strcmp(current->keys[i], department) == 0) {
                University* uni = current->universityLists[i];

                for (int j = 1; j < rank && uni; j++) 
                    uni = uni->next;
                if (uni)
                    printf("%s with score %.1f\n", uni->name, uni->score);
                else
                    printf("Rank not found.\n");

                clock_t end = clock();
                printf("Seek Time : %.6f sec\n", (double)(end - start) / CLOCKS_PER_SEC);

                return;
            }
        }

        current = current->next; // aranan key bu leaf'te yoksa sonraki leaf'e
    }

    printf("Department not found!\n");
}

int getTreeHeight(BPlusNode* node) {
    if (!node) 
        return 0;
    if (node->isLeaf) 
        return 1;

    return 1 + getTreeHeight(node->children[0]);
}

size_t getMemoryUsage(BPlusNode* node) {
    if (!node) 
        return 0;

    size_t total = sizeof(BPlusNode);

    for (int i = 0; i < node->numberKeys; i++) {
        if (node->keys[i]) 
            total += strlen(node->keys[i]) + 1;
        University* uni = node->universityLists[i];

        while (uni) {
            total += sizeof(University);
            uni = uni->next;
        }
    }

    if (!node->isLeaf)
        for (int i = 0; i <= node->numberKeys; i++)
            total += getMemoryUsage(node->children[i]);
    else
        total += getMemoryUsage(node->next);

    return total;
}

void printMetrics() {
    metrics.treeHeight = getTreeHeight(root);
    metrics.memoryBytes = getMemoryUsage(root);
    printf("\nMetrics:\n");
    printf("Tree Height: %d\n", metrics.treeHeight);
    printf("Estimated Memory: %.2f KB\n", metrics.memoryBytes / 1024.0);
    printf("Split Count: %d\n", metrics.splitCount);
    printf("Build Time: %.6f sec\n", metrics.buildTime);

}

void releaseTreeMemory(BPlusNode* node) { // dinamik olarak ayrılan bellek geri verilir
    if (!node) 
        return;

    for (int i = 0; i < node->numberKeys; i++) {
        free(node->keys[i]);
        University* uni = node->universityLists[i];

        while (uni) {
            University* temp = uni;
            uni = uni->next;
            free(temp);
        }
    }

    // internal node un child node ları için release
    if (!node->isLeaf) {
        for (int i = 0; i <= node->numberKeys; i++)
            releaseTreeMemory(node->children[i]);
    }

    free(node->keys);
    free(node->children);
    free(node->universityLists);
    free(node);
}



int main() {
    char fileName[MAX_STR];
    int mode, runCount;

    printf("Please enter CSV file name : ");
    scanf(" %[^\n]", fileName);
    printf("Tree order : ");
    scanf("%d", &order);
    if (order < 3) {
        printf("ERROR: Tree order must be at least 3!\n");

        return 1;
    }
    
    printf("1 - Sequential Insertion\n2 - Bulk Loading\n> ");
    scanf("%d", &mode);
    insertionMode = mode;

    root = NULL;
    metrics = (Metrics){0};

    if (mode == 1) {
        FILE* f = fopen(fileName, "r");
        if (!f) {
            printf("Failed to load the CSV file!\n");

            return 1;
        }

        clock_t start = clock();
        char line[512];
        fgets(line, sizeof(line), f); // header
        while (fgets(line, sizeof(line), f)) {
            Record r;
parseCSVLine(line, &r);
            cleanString(r.uuid);
            cleanString(r.universityName);
            cleanString(r.department);
            insertSequential(r); // veriler tree ye eklenir
            clock_t end = clock();
            metrics.buildTime = (double)(end - start) / CLOCKS_PER_SEC;
        }

        fclose(f);
    } 
    else if (mode == 2) {
        clock_t start = clock();
        generateSortedRuns(fileName, 1000, &runCount);
        mergeSortedRuns("sorted_output.csv", runCount);
        bulkLoadFromSortedCSV("sorted_output.csv");
        clock_t end = clock();
metrics.buildTime = (double)(end - start) / CLOCKS_PER_SEC;
    } 
    else {
        printf("Invalid option!\n");

        return 1;
    }

    printMetrics();

    char department[MAX_STR];
    int rank;
    int choice;

    while (1) {
        printf("\nMENU:\n");
        printf("1 - Search by Department and Rank\n");
        printf("2 - Print B+ Tree Structure\n");
        printf("3 - Exit\n> ");
        scanf("%d", &choice);
        getchar(); // enter temizlenir

        if (choice == 1) {
            printf("Department : ");
            scanf(" %[^\n]", department);
            cleanString(department);
            printf("Rank : ");
            scanf("%d", &rank);
            searchUniRecord(department, rank);
        } 
        else if (choice == 2) {
            printf("\nB+ Tree Structure:\n");
            printTreeStructure(root, 0);
        } 
        else if (choice == 3)
            break;
        else
            printf("Invalid choice!\n");
    }

    if (root) 
        releaseTreeMemory(root); // bellek ram yani serbest bırakılır

    return 0;
}

// Sena İlayda Meral
// Utku Taha Polat
