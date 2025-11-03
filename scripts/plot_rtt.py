### ========================================================================================================================================
## Module       : scripts/plot_rtt.py
## Description  : Plotting utility to visualize client RTTs and server cook-times for Byte-Bistro experiments
## Topics       : Data Visualization, Data Reproducibility, CSV Ingestion, Matplotlib Visualization, Python Integration
## Project      : Byte Bistro (Copyright 2025)
## Purpose      : Reads CSV outputs from client/server runs (GBN & SR, multiple scenarios) and produces:
##                - a 2×2 combined figure (GBN/SR client RTTs + server cook times), and
##                - an optional RTT boxplot for quick distribution comparison.
## Author       : Rizky Johan Saputra (Independent Project)
## Note         : Scenario arg selects file suffixes: baseline | impaired | uniform | exp.
##                Outputs are written to results/figures/*.png with filenames keyed by scenario.
### ========================================================================================================================================

## ======================================================================================================
## SPECIFICATIONS
## ======================================================================================================
## - Input scenarios (argv[1], default="baseline"):
##       - {"baseline", "impaired", "uniform", "exp"}
## - Expected CSV files (under results/):
##       - rtt_gbn_<scenario>_client.csv   -> client RTTs for GBN
##       - cook_gbn_<scenario>_server.csv  -> server cook times for GBN
##       - rtt_sr_<scenario>_client.csv    -> client RTTs for SR
##       - cook_sr_<scenario>_server.csv   -> server cook times for SR
## - CSV schema support:
##       - Two-column generic: id,value
##       - Client header: ts_ms,proto,id,bytes_sent,bytes_recv,rtt_ms
##       - Server header: ts_ms,proto,id,items,cook_ms
## - Parsed output:
##       - Client -> (xs=order_id, ys=rtt_ms)
##       - Server -> (xs=served_id, ys=cook_ms)
## - Outputs:
##       - results/figures/rtt_combined_<scenario>.png   (2×2 panel)
##       - results/figures/rtt_box_<scenario>.png        (boxplot, if any RTT data available)
## - Safety:
##       - Skips malformed lines; prints success paths when files are written.
## - Dependencies:
##       - matplotlib (pyplot), csv, os, sys

## ======================================================================================================
## SETUP (ADJUST IF NECESSARY)
## ======================================================================================================
import csv
import sys
import os
import matplotlib.pyplot as plt

## ======================================================================================================
## IMPLEMENTATIONS
## ======================================================================================================
# Read the csv with the generated log sessions
def read_csv_generic(path):
    xs, ys = [], []
    meta = {"path": path, "columns": []}
    if not os.path.exists(path):
        return xs, ys, meta

    # Declare the path to the file and open for data processing
    with open(path, newline="") as f:
        sniffer = csv.Sniffer()
        sample = f.read(4096)
        f.seek(0)

        # Try to detect header
        has_header = False
        try:
            has_header = sniffer.has_header(sample)
        except Exception:
            pass
        reader = csv.reader(f)
        header = None
        if has_header:
            try:
                header = next(reader)
            except StopIteration:
                return xs, ys, meta
            meta["columns"] = header

        # Declare a mapping by its name if we have a header
        if header:
            # Normalize the data from the csv
            low = [c.strip().lower() for c in header]
            col_idx = {c: i for i, c in enumerate(low)}
            is_client = ("id" in col_idx and "rtt_ms" in col_idx)
            is_server = ("id" in col_idx and "cook_ms" in col_idx)
            for row in reader:
                if not row:
                    continue
                try:
                    if is_client:
                        x = int(float(row[col_idx["id"]]))
                        y = float(row[col_idx["rtt_ms"]])
                        xs.append(x); ys.append(y)
                    elif is_server:
                        x = int(float(row[col_idx["id"]]))
                        y = float(row[col_idx["cook_ms"]])
                        xs.append(x); ys.append(y)
                    else:
                        # Unknown header; try 2-col fallback
                        if len(row) >= 2:
                            xs.append(int(float(row[0])))
                            ys.append(float(row[1]))
                except Exception:
                    # Skip malformed lines
                    pass
        else:
            # No header (Assume 2 columns id,rtt)
            for row in reader:
                if len(row) < 2:
                    continue
                try:
                    xs.append(int(float(row[0])))
                    ys.append(float(row[1]))
                except Exception:
                    pass

    return xs, ys, meta

# Initialize the plot layout and format
def plot_xy(ax, xs, ys, title, xlabel, ylabel):
    if xs and ys:
        ax.plot(xs, ys, marker=".", linewidth=1)
        ax.set_title(title)
        ax.set_xlabel(xlabel)
        ax.set_ylabel(ylabel)
        ax.grid(True, which="both", axis="both", linestyle="--", linewidth=0.5)
    else:
        ax.set_title(title + " (No data)")
        ax.set_xlabel(xlabel)
        ax.set_ylabel(ylabel)
        ax.grid(True, which="both", axis="both", linestyle="--", linewidth=0.5)

# Declare the main function to generate the appended figure
def main():
    # Declare the scenario to determine the filenames suffix
    scenario = (sys.argv[1] if len(sys.argv) > 1 else "baseline").lower()
    valid = {"baseline", "impaired", "uniform", "exp"}
    if scenario not in valid:
        print(f"[ERR] scenario must be one of: {sorted(valid)}")
        sys.exit(1)

    # Declare the expected file names (Adjust accordingly)
    paths = {
        "gbn_client": f"results/rtt_gbn_{scenario}_client.csv",
        "gbn_server": f"results/cook_gbn_{scenario}_server.csv",
        "sr_client":  f"results/rtt_sr_{scenario}_client.csv",
        "sr_server":  f"results/cook_sr_{scenario}_server.csv",
    }

    # Load all the csv data
    gbn_c_x, gbn_c_y, _ = read_csv_generic(paths["gbn_client"])
    gbn_s_x, gbn_s_y, _ = read_csv_generic(paths["gbn_server"])
    sr_c_x,  sr_c_y,  _ = read_csv_generic(paths["sr_client"])
    sr_s_x,  sr_s_y,  _ = read_csv_generic(paths["sr_server"])

    # Declare the appended 2×2 figure
    fig, axs = plt.subplots(2, 2, figsize=(12, 8))
    fig.suptitle(f"RTT & Cook-Time — {scenario.upper()}")

    # Top-left (GBN client RTT)
    plot_xy(axs[0, 0], gbn_c_x, gbn_c_y,
            "GBN — Client RTT", "order id", "RTT (ms)")
    
    # Top-right (GBN server cook)
    plot_xy(axs[0, 1], gbn_s_x, gbn_s_y,
            "GBN — Server Cook Time", "served id", "Cook time (ms)")
    
    # Bottom-left (SR client RTT)
    plot_xy(axs[1, 0], sr_c_x, sr_c_y,
            "SR — Client RTT", "order id", "RTT (ms)")
    
    # Bottom-right (SR server cook)
    plot_xy(axs[1, 1], sr_s_x, sr_s_y,
            "SR — Server Cook Time", "served id", "Cook time (ms)")

    # Layout the visual graph with the specified dimensions and names
    plt.tight_layout(rect=[0, 0.03, 1, 0.95])
    os.makedirs("results", exist_ok=True)
    out_png = f"results/figures/rtt_combined_{scenario}.png"
    plt.savefig(out_png, dpi=150)
    print(f"[OK] wrote {out_png}")

    # Emit a small 4-box boxplot comparison (client RTTs only)
    combos = [("GBN client", gbn_c_y), ("SR client", sr_c_y)]
    data = [ys for _, ys in combos if ys]
    labels = [lbl for lbl, ys in combos if ys]
    if data:
        plt.figure(figsize=(6, 4))
        plt.boxplot(data, labels=labels, showfliers=False)
        plt.ylabel("RTT (ms)")
        plt.title(f"Client RTT distribution — {scenario.upper()}")
        plt.grid(True, axis="y", linestyle="--", linewidth=0.5)
        out_box = f"results/figures/rtt_box_{scenario}.png"
        plt.tight_layout()
        plt.savefig(out_box, dpi=150)
        print(f"[OK] wrote {out_box}")

## ======================================================================================================
## DEMO AND TESTING
## ======================================================================================================
if __name__ == "__main__":
    main()

### ========================================================================================================================================
## END (ADD IMPLEMENTATIONS IF NECESSARY)
### ========================================================================================================================================