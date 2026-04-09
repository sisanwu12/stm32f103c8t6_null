{
  description = "Embedded MCU development environment with Nix";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs {
        inherit system;
        config.allowUnfree = true;
      };
      llvmPkgs = pkgs.llvmPackages;
    in {
      devShells.${system}.default = pkgs.mkShell {
        packages = with pkgs; [

          # C 基础工具链
          cmake
          ninja
          gcc-arm-embedded
          openocd
          llvmPkgs.clang
          llvmPkgs.clang-tools
          gnumake
          git
          pkg-config

          # Rust 基础工具链
          rustc
          cargo
          rust-analyzer
          rustfmt
          clippy

          # C / Rust 混写常用
          # cbindgen
          rustup
        ];

        nativeBuildInputs = with pkgs; [
          rustPlatform.bindgenHook
        ];

        RUST_SRC_PATH = "${pkgs.rustPlatform.rustLibSrc}";

        shellHook = ''
          if [ -t 1 ]; then
            echo "Embedded dev shell is ready."
            echo "Tools:"
            echo "  cmake     -> $(command -v cmake || echo not-found)"
            echo "  ninja     -> $(command -v ninja || echo not-found)"
            echo "  clang     -> $(command -v clang || echo not-found)"
            echo "  arm-none-eabi-gcc -> $(command -v arm-none-eabi-gcc || echo not-found)"
            echo "  openocd   -> $(command -v openocd || echo not-found)"
            echo "  cargo     -> $(command -v cargo || echo not-found)"
            echo "  rustc     -> $(command -v rustc || echo not-found)"
            echo "  rust-analyzer -> $(command -v rust-analyzer || echo not-found)"
            echo "  cbindgen  -> $(command -v cbindgen || echo not-found)"
            echo "  rustup    -> $(command -v rustup || echo not-found)"
          fi
        '';
      };
    };
}