# YOK Atlas B+ Tree

This project is a C-based implementation of a **B+ Tree** built on YOK Atlas university placement data. It was developed for **CME2202 Assignment 2** to explore indexing, linked data structures, and external sorting using a real-world CSV dataset. The program reads records from `yok_atlas.csv`, organizes them by **department name**, and stores the universities of each department in a linked list sorted by **descending base placement score**. :contentReference[oaicite:5]{index=5} :contentReference[oaicite:6]{index=6}

The application supports two tree construction methods. In **Sequential Insertion**, records are inserted one by one into the B+ Tree while preserving balance and splitting nodes when necessary. In **Bulk Loading**, the dataset is first sorted externally, intermediate run files are generated, the runs are merged into `sorted_output.csv`, and the B+ Tree is then built level by level from the sorted data. :contentReference[oaicite:7]{index=7} :contentReference[oaicite:8]{index=8} :contentReference[oaicite:9]{index=9}

Leaf nodes store department names and point to linked lists of universities, while internal nodes are used for navigation. Leaf nodes are also linked to each other, allowing ordered traversal across departments. During execution, the program reports key metrics such as **tree height**, **estimated memory usage**, **split count**, and **build time**. :contentReference[oaicite:10]{index=10} :contentReference[oaicite:11]{index=11} :contentReference[oaicite:12]{index=12}

After construction, the user can search by entering a **department name** and a **rank**. The program then returns the university at that position in the department’s linked list together with its score, and also prints the seek time for that search. It can additionally print the full B+ Tree structure through a menu-based interface. :contentReference[oaicite:13]{index=13} :contentReference[oaicite:14]{index=14} :contentReference[oaicite:15]{index=15}

## Files

- `yok-atlas-bplus-tree.c` — main source code
- `yok_atlas.csv` — input dataset
- `Assignment2.pdf` — assignment description

## Compilation

```bash
gcc yok-atlas-bplus-tree.c -o yokatlas
Usage
./yokatlas

When the program starts, it asks for:

the CSV file name
the B+ Tree order
the loading method:
1 Sequential Insertion
2 Bulk Loading

After the tree is built, the program shows a menu where you can:

search by department and rank
print the B+ Tree structure
exit the program
Output Metrics

The program reports:

Tree Height
Estimated Memory Usage
Split Count
Build Time

For search operations, it also reports:

Seek Time
