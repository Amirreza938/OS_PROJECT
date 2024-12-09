import tkinter as tk
from tkinter import ttk, messagebox
from tkinter.simpledialog import askfloat

class OrderManagerApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Order Manager")

        self.orders = []

        # Username
        tk.Label(root, text="Username:").grid(row=0, column=0, padx=5, pady=5)
        self.username_entry = tk.Entry(root)
        self.username_entry.grid(row=0, column=1, padx=5, pady=5)

        # Number of Orders
        tk.Label(root, text="Number of Orders:").grid(row=1, column=0, padx=5, pady=5)
        self.num_orders_spinbox = tk.Spinbox(root, from_=1, to=100, width=10)
        self.num_orders_spinbox.grid(row=1, column=1, padx=5, pady=5)

        # Orders List
        tk.Label(root, text="Order List:").grid(row=2, column=0, padx=5, pady=5)
        self.order_listbox = tk.Listbox(root, width=40, height=10)
        self.order_listbox.grid(row=2, column=1, padx=5, pady=5)

        # Add Order
        self.add_order_button = tk.Button(root, text="Add Order", command=self.add_order)
        self.add_order_button.grid(row=3, column=1, pady=5)

        # Price Threshold
        tk.Label(root, text="Price Threshold:").grid(row=4, column=0, padx=5, pady=5)
        self.price_threshold_entry = tk.Entry(root)
        self.price_threshold_entry.grid(row=4, column=1, padx=5, pady=5)

        # Submit Button
        self.submit_button = tk.Button(root, text="Submit", command=self.submit)
        self.submit_button.grid(row=5, column=1, pady=10)

    def add_order(self):
        """Opens a dialog to add an order."""
        add_order_window = tk.Toplevel(self.root)
        add_order_window.title("Add Order")

        tk.Label(add_order_window, text="Product Name:").grid(row=0, column=0, padx=5, pady=5)
        product_name_entry = tk.Entry(add_order_window)
        product_name_entry.grid(row=0, column=1, padx=5, pady=5)

        tk.Label(add_order_window, text="Quantity:").grid(row=1, column=0, padx=5, pady=5)
        quantity_spinbox = tk.Spinbox(add_order_window, from_=1, to=100, width=10)
        quantity_spinbox.grid(row=1, column=1, padx=5, pady=5)

        def save_order():
            product_name = product_name_entry.get()
            try:
                quantity = int(quantity_spinbox.get())
                if not product_name.strip():
                    raise ValueError("Product name cannot be empty")
                self.orders.append((product_name, quantity))
                self.order_listbox.insert(tk.END, f"{product_name} ({quantity})")
                add_order_window.destroy()
            except ValueError as e:
                messagebox.showerror("Error", f"Invalid input: {e}")

        tk.Button(add_order_window, text="Save", command=save_order).grid(row=2, column=1, pady=5)

    def submit(self):
        """Validates and submits the data."""
        username = self.username_entry.get().strip()
        num_orders = int(self.num_orders_spinbox.get())
        price_threshold = self.price_threshold_entry.get()

        if not username:
            messagebox.showerror("Error", "Username is required")
            return

        if len(self.orders) != num_orders:
            messagebox.showerror("Error", "Number of orders doesn't match")
            return

        try:
            price_threshold = float(price_threshold)
        except ValueError:
            messagebox.showerror("Error", "Invalid price threshold")
            return

        # Print data to simulate handling in the original C program
        print(f"Username: {username}")
        print(f"Orders:")
        for product_name, quantity in self.orders:
            print(f"- {product_name}: {quantity}")
        print(f"Price Threshold: {price_threshold}")

        # Display a success message
        messagebox.showinfo("Success", "Data submitted successfully!")

if __name__ == "__main__":
    root = tk.Tk()
    app = OrderManagerApp(root)
    root.mainloop()
