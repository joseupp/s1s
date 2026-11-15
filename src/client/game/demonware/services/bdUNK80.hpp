#pragma once

namespace demonware
{
	class bdUNK80 final : public service
	{
	public:
		bdUNK80();

	protected:
		bool exec_missing_task(service_server* server, byte_buffer* buffer) override;

	private:
		void unk42(service_server* server, byte_buffer* buffer) const;
		void unk49(service_server* server, byte_buffer* buffer) const;
		void unk60(service_server* server, byte_buffer* buffer) const;
		void unk130(service_server* server, byte_buffer* buffer) const;
		void get_inventory(service_server* server, byte_buffer* buffer) const;
		void unk193(service_server* server, byte_buffer* buffer) const;
		void get_coupons(service_server* server, byte_buffer* buffer) const;
		void claim_coupons(service_server* server, byte_buffer* buffer) const;
		void redeem_coupons(service_server* server, byte_buffer* buffer) const;
		void consume_item(service_server* server, byte_buffer* buffer) const;
		void set_item_data(service_server* server, byte_buffer* buffer) const;
		void delete_items(service_server* server, byte_buffer* buffer) const;
	};
}
