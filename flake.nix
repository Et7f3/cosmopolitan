{
  description = "A very basic flake";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs?ref=nixpkgs-unstable";

  outputs = { self, nixpkgs, }:
    let
      # System types to support.
      supportedSystems = [ "x86_64-linux" "x86_64-darwin" ];

      # Helper function to generate an attrset '{ x86_64-linux = f "x86_64-linux"; ... }'.
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;

      # Nixpkgs instantiated for supported system types.
      nixpkgsFor = forAllSystems (system: import nixpkgs { inherit system; });

      nixpkgsForCross = forAllSystems (system:
        import nixpkgs {
          inherit system;
          crossSystem = "x86_64-linux";
        });
    in {
      packages = forAllSystems (system: {
        # Join all binary under the same prefix
        sysroot = nixpkgsFor.${system}.symlinkJoin {
          name = "cosmopolitan-sysroot";
          paths = with nixpkgsForCross.${system}.stdenv; [
            cc.cc
            cc.bintools.bintools
          ];
        };

        # used by nix develop -i
        # TODO: create a proper derivation that build
        default = nixpkgsFor.${system}.stdenvNoCC.mkDerivation {
          name = "cosmopolitan";
          PREFIX = "${self.packages.${system}.sysroot}/bin/${
              nixpkgsForCross.${system}.stdenv.cc.targetPrefix
            }";
        };
      });

      formatter = forAllSystems (system: nixpkgsFor.${system}.nixfmt);
    };
}
