# OS_PROJECT

This project is a comprehensive multithreaded shop management system. The aim is to simulate the functionalities of a store in a multithreaded environment, handling complex operations like dataset management, user interactions, multithreaded processing of orders, discount handling, product ranking, and logging, all while ensuring optimal performance and reliability.

**______________________________________________________________________________________________________**

**Introduction**

Our project tackles the problem of efficiently managing a shop that has multiple stores, categories, and products. By leveraging multithreading and inter-process communication, the system ensures high responsiveness and accuracy. It supports features like dataset loading, concurrent product processing, best store selection, logging mechanisms, discount handling, and customer feedback integration.

This project demonstrates:

The **effective use of multithreading** to process large datasets and user requests simultaneously.

**File and data structure handling**, ensuring data is loaded and managed hierarchically (store → category → product).

A focus on **user experience**, with features like discounts for repeat customers and opportunities to provide ratings.

**______________________________________________________________________________________________________**

**Key Features**

**1. Dataset Loading:**

A hierarchical data structure is used to represent:

**Stores:** Each store has multiple categories.

**Categories:** Each category groups related products.

**Products:** Contains information like name, price, score, entity count and last modification.

The dataset is loaded from files and directories, ensuring scalability and modularity. Users can explore the data in a structured manner.

**2. Multithreading and Process Management:**

Threads are used to handle concurrent product checks for orders, ensuring that the system processes multiple products simultaneously.

Separate processes are spawned for handling stores and categories, isolating workload and optimizing CPU usage.

**3. Best Store Selection:**

Compares stores based on product availability, user-defined price thresholds, and calculated scores.
Dynamically ranks stores to help users make informed purchasing decisions.

**4. Logging:**

Real-time, user-specific logs to track all activities during a session.
Centralized log maintenance for debugging and audit purposes.

**5. Discount System:**

Rewards repeat customers with discounts, promoting customer retention.
Discounts are applied automatically based on purchase history.

**6.User Ratings:**

Enables users to provide feedback by rating purchased products.
Dynamically updates product scores based on user ratings.

**______________________________________________________________________________________________________**

**How It Works**

**1. Dataset Handling**

The first step in the project is to load the dataset.

A hierarchical structure is used, allowing the system to navigate from stores to categories and finally to products.

The dataset is parsed from directory paths, where each store corresponds to a folder, each category to a subfolder, and each product to a file.

Products contain attributes such as:

**Name:** The product's name.
    
**Price:** The cost of the product.

**Score:** A dynamic rating influenced by user feedback.

**Entity Count:** The quantity of the product available in stock.

**Last modified:** The last modification of text file of the product which its score or entity changed.

Example Structure:

    Store: Store1
       >>>> Category: Apparel
           >>>> Product: Jeans
           >>>> Product: T-shirt
       >>>> Category: Beauty
           >>>> Product: Foundation
           >>>> Product: Lipstick

The data_load.h file handles the parsing and representation of this structure.

**______________________________________________________________________________________________________**

**2. Multithreading**

**Threads for Product Handling:**

**a.** Each product in a category is processed by a separate thread.

**b.** Threads check product availability against user orders.

**c.** Uses a mutex to ensure thread-safe logging and shared resource access.

**Processes for Stores and Categories:**

**a.** Each store is managed by a separate process, isolating workload.

**b.** Child processes handle categories within a store, allowing efficient parallel processing.

**______________________________________________________________________________________________________**

**3. Best Store Selection**

**Store Evaluation:** Each store is evaluated based on:

**a.** The total price of the selected products.

**b.** The completeness of the user’s order (does the store have all requested items?).

**c.** A weighted score calculated from product ratings and prices.

**Inter-Process Communication:** Pipes are used to send data from child processes to the main process for evaluation.

Result: The store with the highest satisfied criteria that meets the user-defined price threshold is selected.

**______________________________________________________________________________________________________**

**4. Logging**

Logging is a critical part of the system, ensuring traceability and transparency.

**Features:**

**a.** Each user session generates a unique log file capturing all actions performed.

**b.** Activities like thread creation, product processing, and purchase decisions are recorded with timestamps.

**c.** User-specific logs maintain a history of all purchases, which is also used to apply discounts for repeat customers.

**______________________________________________________________________________________________________**

**5. Discount System**

To enhance customer retention, a discount mechanism is implemented:

**a.** Users receive a 5% discount when revisiting a store they have purchased from previously.

**b.** Purchase history is stored in a user-specific file.

**c.** The discount is applied automatically during the billing phase.

**______________________________________________________________________________________________________**

**6. User Ratings**

After completing a purchase, users can rate the products they bought:

**a.** Ratings are on a scale of 1–5.

**b.** Product scores are dynamically updated as the average of all received ratings.

**c.** Threads handle the rating updates, ensuring efficient score recalculation.

**______________________________________________________________________________________________________**

**Technical Highlights**

**1. Concurrency:**

**a.** Threads and processes are used to handle workloads in parallel.

**b.** Mutex locks prevent race conditions in shared resources like logs.

**2. File Handling:**

**a.** Dynamic dataset loading from directories and files.

**b.** Efficient use of file pointers for reading and writing product data.

**3. Inter-Process Communication:**

**a.** Pipes are used for transferring data between parent and child processes.

**4. Dynamic Updates:**

**a.** Product scores and stock counts are updated in real time based on user interactions.

**______________________________________________________________________________________________________**

**Future Enhancements**

**1.** Expand the dataset structure to include product images and detailed descriptions.

**2.** Add a graphical user interface for easier interaction.

**3.** Introduce a recommendation system based on purchase history.

**4.** Implement a real-time notification system for stock updates.

**5.** Optimize thread pool management for better performance on large datasets.

**______________________________________________________________________________________________________**



    
