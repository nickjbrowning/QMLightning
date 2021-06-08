#include <torch/extension.h>
#include<iostream>

using namespace at;
using namespace std;

void getElementTypesCuda(torch::Tensor coordinates, torch::Tensor charges, torch::Tensor natoms, torch::Tensor species, torch::Tensor element_types);

void EGTOCuda(torch::Tensor coordinates, torch::Tensor charges, torch::Tensor species, torch::Tensor element_types, torch::Tensor blockAtomIDs,
		torch::Tensor blockMolIDs, torch::Tensor neighbourlist, torch::Tensor nneighbours, torch::Tensor mbodylist, torch::Tensor gto_components,
		torch::Tensor gto_powers, torch::Tensor orbital_weights, torch::Tensor gridpoints, float eta, int lmax, float rcut, torch::Tensor gto_output);

void EGTODerivativeCuda(torch::Tensor coordinates, torch::Tensor charges, torch::Tensor species, torch::Tensor element_types, torch::Tensor blockAtomIDs,
		torch::Tensor blockMolIDs, torch::Tensor neighbourlist, torch::Tensor nneighbours, torch::Tensor mbodylist, torch::Tensor gto_components,
		torch::Tensor gto_powers, torch::Tensor orbital_weights, torch::Tensor gridpoints, float eta, int lmax, float rcut, torch::Tensor gto_output,
		torch::Tensor gto_output_derivative);

torch::Tensor get_element_types_gpu(torch::Tensor coordinates, torch::Tensor charges, torch::Tensor natom_counts, torch::Tensor species) {

	TORCH_CHECK(coordinates.device().type() == torch::kCUDA, "coordinates must be a CUDA tensor");

	TORCH_CHECK(charges.device().type() == torch::kCUDA, "charges must be a CUDA tensor");

	TORCH_CHECK(species.device().type() == torch::kCUDA, "species must be a CUDA tensor");

	if (coordinates.dim() == 2) { // pad a dimension so elementalGTOGPUSharedMem still works

		coordinates = coordinates.unsqueeze(0);
		charges = charges.unsqueeze(0);

	}

	int nbatch = coordinates.size(0);
	int natoms = coordinates.size(1);

	auto options = torch::TensorOptions().dtype(torch::kInt32).layout(torch::kStrided).device(torch::kCUDA);

	torch::Tensor element_types = torch::zeros( { nbatch, natoms }, options);

	getElementTypesCuda(coordinates, charges, natom_counts, species, element_types);

	return element_types;
}

std::vector<torch::Tensor> get_egto(torch::Tensor coordinates, torch::Tensor charges, torch::Tensor species, torch::Tensor element_types,
		torch::Tensor blockAtomIDs, torch::Tensor blockMolIDs, torch::Tensor neighbourlist, torch::Tensor nneighbours, torch::Tensor mbodylist,
		torch::Tensor gto_components, torch::Tensor orbital_weights, torch::Tensor gto_powers, torch::Tensor gridpoints, float eta, int lmax, float rcut,
		bool gradients) {

	/** ElementalGTO representation GPU wrapper
	 *
	 * coordinates: [nbatch, natoms, 3]
	 * charges: [nbatch, natoms]
	 * element_types: [nbatch, natoms]
	 * mbodylist: [nspecies, nspecies]
	 * gto_components: [norbs, 3]
	 * orbital_weights: [norbs]
	 * gto_powers: [norbs]
	 * gridpoints: [ngaussians]
	 *
	 * **/

	TORCH_CHECK(coordinates.device().type() == torch::kCUDA, "coordinates must be a CUDA tensor");

	TORCH_CHECK(charges.device().type() == torch::kCUDA, "charges must be a CUDA tensor");

	TORCH_CHECK(element_types.device().type() == torch::kCUDA, "element_types must be a CUDA tensor");

	TORCH_CHECK(mbodylist.device().type() == torch::kCUDA, "mbodylist must be a CUDA tensor");

	TORCH_CHECK(species.device().type() == torch::kCUDA, "species must be a CUDA tensor");

	TORCH_CHECK(gto_components.device().type() == torch::kCUDA, "gto_components must be a CUDA tensor");

	TORCH_CHECK(orbital_weights.device().type() == torch::kCUDA, "orbital_weights must be a CUDA tensor");

	TORCH_CHECK(gto_powers.device().type() == torch::kCUDA, "gto_powers must be a CUDA tensor");

	TORCH_CHECK(gridpoints.device().type() == torch::kCUDA, "gridpoints must be a CUDA tensor");

	torch::Tensor clone_coordinates;
	torch::Tensor clone_charges;
	torch::Tensor clone_element_types;

	int nspecies = mbodylist.size(0);

	int ngaussians = gridpoints.size(0);

	int nmbody = int(float((nspecies + 1) / 2.0) * nspecies);
	int repsize = nmbody * (lmax + 1) * ngaussians;

	if (coordinates.dim() == 2) { // pad a dimension so elementalGTOGPUSharedMem still works

		coordinates = coordinates.unsqueeze(0);
		charges = charges.unsqueeze(0);
	}

	int nbatch = coordinates.size(0);
	int natoms = coordinates.size(1);

	auto options = torch::TensorOptions().dtype(torch::kFloat32).layout(torch::kStrided).device(torch::kCUDA);

	if (gradients) {

		torch::Tensor gto_output = torch::zeros( { nbatch, natoms, repsize }, options);
		torch::Tensor gto_output_derivative = torch::zeros( { nbatch, natoms, natoms, 3, repsize }, options);

		EGTODerivativeCuda(coordinates, charges, species, element_types, blockAtomIDs, blockMolIDs, neighbourlist, nneighbours, mbodylist, gto_components,
				gto_powers, orbital_weights, gridpoints, eta, lmax, rcut, gto_output, gto_output_derivative);

		return {gto_output, gto_output_derivative};
	} else {

		torch::Tensor gto_output = torch::zeros( { nbatch, natoms, repsize }, options);

		EGTOCuda(coordinates, charges, species, element_types, blockAtomIDs, blockMolIDs, neighbourlist, nneighbours, mbodylist, gto_components, gto_powers,
				orbital_weights, gridpoints, eta, lmax, rcut, gto_output);

		return {gto_output};
	}
}

PYBIND11_MODULE(TORCH_EXTENSION_NAME, m)
{
	m.def("get_egto", &get_egto, "Elemental GTO Representation in Shared Memory");
	m.def("get_element_types_gpu2", &get_element_types_gpu, "returns atomic species according to torch::Tensor species");

}