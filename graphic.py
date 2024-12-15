from PyQt5.QtWidgets import (
    QApplication, QWidget, QLabel, QLineEdit, QVBoxLayout, QPushButton, QTableWidget,
    QTableWidgetItem, QSpinBox, QHBoxLayout, QMessageBox, QStackedLayout
)
from PyQt5.QtCore import Qt
import subprocess
import sys
class FrontEndApp(QWidget):
    def __init__(self):
        super().__init__()
        self.store_values = {}  # Initialize store_values here
        self.current_store = None  # Initialize current_store here
        self.initUI()

    def initUI(self):
        self.setWindowTitle("Order Processing Frontend")
        self.setGeometry(200, 200, 600, 400)

        self.stack = QStackedLayout()
        self.input_page = QWidget()
        self.results_page = QWidget()

        self.setup_input_page()
        self.setup_results_page()

        self.stack.addWidget(self.input_page)
        self.stack.addWidget(self.results_page)

        main_layout = QVBoxLayout()
        main_layout.addLayout(self.stack)
        self.setLayout(main_layout)

    def setup_input_page(self):
        layout = QVBoxLayout()

        layout.addWidget(QLabel("Username:"))
        self.username_input = QLineEdit()
        layout.addWidget(self.username_input)

        layout.addWidget(QLabel("Number of Orders:"))
        self.order_count_input = QSpinBox()
        self.order_count_input.setMinimum(1)
        layout.addWidget(self.order_count_input)

        layout.addWidget(QLabel("Orders (Product Name and Quantity):"))
        self.order_table = QTableWidget(1, 2)
        self.order_table.setHorizontalHeaderLabels(["Product Name", "Quantity"])
        layout.addWidget(self.order_table)

        buttons_layout = QHBoxLayout()
        add_row_btn = QPushButton("Add Row")
        add_row_btn.clicked.connect(self.add_row)
        remove_row_btn = QPushButton("Remove Row")
        remove_row_btn.clicked.connect(self.remove_row)
        buttons_layout.addWidget(add_row_btn)
        buttons_layout.addWidget(remove_row_btn)
        layout.addLayout(buttons_layout)

        layout.addWidget(QLabel("Price Threshold (Optional):"))
        self.price_threshold_input = QLineEdit()
        layout.addWidget(self.price_threshold_input)

        submit_btn = QPushButton("Submit")
        submit_btn.clicked.connect(self.submit_data)
        layout.addWidget(submit_btn)

        self.input_page.setLayout(layout)

    def setup_results_page(self):
        layout = QVBoxLayout()

        self.results_table = QTableWidget()
        self.results_table.setColumnCount(3)
        self.results_table.setHorizontalHeaderLabels(["Store", "Aggregated Value", "Orders"])
        layout.addWidget(self.results_table)
        self.best_store_label = QLabel("")
        layout.addWidget(self.best_store_label)
        self.purshase_label = QLabel("")
        layout.addWidget(self.purshase_label)
        button_layout = QHBoxLayout()
        self.buy_button = QPushButton("Buy")
        self.buy_button.clicked.connect(self.buy_from_store)
        self.decline_button = QPushButton("Decline")
        self.buy_button.clicked.connect(self.decline_purchase)
        button_layout.addWidget(self.buy_button)
        button_layout.addWidget(self.decline_button)
        layout.addLayout(button_layout)
        back_btn = QPushButton("Back")
        back_btn.clicked.connect(self.back_to_input)
        layout.addWidget(back_btn)

        self.results_page.setLayout(layout)

    def add_row(self):
        self.order_table.insertRow(self.order_table.rowCount())

    def remove_row(self):
        current_row = self.order_table.rowCount()
        if current_row > 1:
            self.order_table.removeRow(current_row - 1)

    def back_to_input(self):
        self.stack.setCurrentWidget(self.input_page)

    def submit_data(self):
        username = self.username_input.text()
        order_count = self.order_count_input.value()
        price_threshold = self.price_threshold_input.text()

        if not username:
            QMessageBox.warning(self, "Input Error", "Username is required.")
            return

        try:
            price_threshold = float(price_threshold) if price_threshold else -1
        except ValueError:
            QMessageBox.warning(self, "Input Error", "Invalid price threshold.")
            return

        orders = []
        for row in range(self.order_table.rowCount()):
            name_item = self.order_table.item(row, 0)
            quantity_item = self.order_table.item(row, 1)
            if not name_item or not quantity_item:
                QMessageBox.warning(self, "Input Error", "All order rows must be filled.")
                return
            orders.append((name_item.text(), quantity_item.text()))

        input_data = f"{username}\n{order_count}\n"
        for name, qty in orders:
            input_data += f"{name} {qty}\n"
        input_data += f"{price_threshold}\n"

        try:
            process = subprocess.Popen(
                ["./user_process"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
            )
            output, error = process.communicate(input=input_data)

            if process.returncode != 0:
                QMessageBox.critical(self, "Error", error)
                return

            self.parse_results(output)
        except FileNotFoundError:
            QMessageBox.critical(self, "Error", "C program not found.")
        except Exception as e:
            QMessageBox.critical(self, "Error", f"An error occurred: {e}")
 
    def parse_results(self, output):
        self.results_table.setRowCount(0)
        best_store = None
        highest_value = -float('inf')  # Initialize to a very low value

        for line in output.splitlines():
            if line.startswith("Store:"):
                try:
                    parts = line.split(", ")
                    store = parts[0].split(": ")[1]
                    total_price = float(parts[1].split(": ")[1])  # Ensure it's a float
                    orders = ", ".join(parts[2:]).split(": ")[1]  # Handles multiple orders correctly

                # Insert the store result into the table
                    row = self.results_table.rowCount()
                    self.results_table.insertRow(row)
                    self.results_table.setItem(row, 0, QTableWidgetItem(store))
                    self.results_table.setItem(row, 1, QTableWidgetItem(f"{total_price:.2f}"))
                    self.results_table.setItem(row, 2, QTableWidgetItem(orders))
                    self.store_values[store] = {
                        "total_price" : total_price,
                        "orders" : orders,
                        "value" : total_price
                    }
                # Compare to find the store with the highest value
                    if total_price > highest_value:
                        highest_value = total_price
                        best_store = store

                except (IndexError, ValueError):
                    QMessageBox.warning(self, "Error", f"Unexpected output format: {line}")

    # After all stores are processed, show the best store
        if best_store:
            self.best_store_label.setText(f"The best store is: {best_store} with value: {highest_value:.2f}")
            self.purshase_label.setText(f"Do you want to buy from: {best_store}?")
            self.current_store = best_store
            #QMessageBox.information(self, "Best Store", f"The best store is: {best_store} with value: {highest_value:.2f}")
        else:
            self.best_store_label.setText("The best store is not found")
            self.purshase_label.setText("")
            self.current_store = None
        self.stack.setCurrentWidget(self.results_page)
    def buy_from_store(self):
        if self.current_store is None:
            QMessageBox.warning(self, "Purchase Error", "No store selected.")
            return

        current_store_details = self.store_values[self.current_store]
        total_price = current_store_details["total_price"]

        price_threshold = self.price_threshold_input.text()
        price_threshold = float(price_threshold) if price_threshold else -1

        if price_threshold > 0 and total_price > price_threshold:
        # Find next suitable store
            for store, details in sorted(self.store_values.items(), key=lambda x: -x[1]["value"]):
                if store != self.current_store and details["total_price"] <= price_threshold:
                    self.current_store = store
                    self.best_store_label.setText(f"The best store is: {store} with value: {details['total_price']:.2f}")
                    self.purshase_label.setText(f"Do you want to buy from: {store}?")
                    return

            QMessageBox.warning(self, "Purchase Error", "No suitable store found within the price threshold.")
            return

    def decline_purchase(self):
        QMessageBox.information(self, "Purchase Declined", "You declined the purchase.")
        self.back_to_input()

  

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = FrontEndApp()
    window.show()
    sys.exit(app.exec_())
